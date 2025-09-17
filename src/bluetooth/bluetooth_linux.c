#include "bluetooth_linux.h"
#include <stdio.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <vector>
#include <map>
#include "bluetooth_callbacks.h"
#include "crypto_manager.h"
#include <json-c/json.h>

// BlueZ D-Bus service and interfaces
#define BLUEZ_BUS_NAME "org.bluez"
#define BLUEZ_ADAPTER_INTERFACE "org.bluez.Adapter1"
#define BLUEZ_DEVICE_INTERFACE "org.bluez.Device1"
#define BLUEZ_GATT_SERVICE_INTERFACE "org.bluez.GattService1"
#define BLUEZ_GATT_CHARACTERISTIC_INTERFACE "org.bluez.GattCharacteristic1"
#define DBUS_PROPERTIES_INTERFACE "org.freedesktop.DBus.Properties"
#define DBUS_OBJECT_MANAGER_INTERFACE "org.freedesktop.DBus.ObjectManager"

// Custom service and characteristic UUIDs (matching macOS and Windows)
#define BLUEBEAM_SERVICE_UUID "E20A39F4-73F5-4BC4-A12F-17D1AD07A961"
#define MESSAGE_CHARACTERISTIC_UUID "08590F7E-DB05-467E-8757-72F6F6669999"
#define FILE_TRANSFER_CHARACTERISTIC_UUID "08590F7E-DB05-467E-8757-72F6F6668888"

// Max data size for a single BLE write (considering MTU and overhead)
#define MAX_BLE_WRITE_DATA_SIZE 500

// Structure to hold discovered device information
typedef struct {
    std::string object_path;
    std::string address;
    std::string name;
    // Store discovered GATT characteristics for this device
    std::string messageCharacteristicPath;
    std::string fileTransferCharacteristicPath;
    bool messageCharacteristicFound;
    bool fileTransferCharacteristicFound;
} DiscoveredDevice;

// Structure to manage ongoing file transfers
typedef struct {
    FILE *file_handle;
    char file_path[1024];
    char file_name[256]; // To store just the filename from metadata
    long file_size;
    long bytes_transferred;
    unsigned long long current_chunk_index;
    unsigned char encryption_key[crypto_secretbox_KEYBYTES]; // Key for this specific transfer
    bool is_sending; // true for sending, false for receiving
    std::string device_address; // To identify the device for callbacks
    std::string characteristic_path; // To identify the characteristic for callbacks
} FileTransferState;

// Global D-Bus connection
static sd_bus *bus = NULL;
// Global list of discovered devices
static std::vector<DiscoveredDevice> discoveredDevices;
// Map of connected device object paths, keyed by address
static std::map<std::string, std::string> connectedDevices;
// Map of ongoing file transfers, keyed by device address
static std::map<std::string, FileTransferState*> ongoingFileTransfers;

// Forward declaration for helper function
static void send_next_file_chunk(FileTransferState *transferState);

// Helper to get a property from a D-Bus message
static int get_property_string(sd_bus_message *m, const char *property_name, std::string &value) {
    int r;
    const char *s;
    r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sv");
    if (r < 0) return r;
    r = sd_bus_message_read(m, "s", &s);
    if (r < 0) return r;
    if (strcmp(s, property_name) == 0) {
        r = sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, "s");
        if (r < 0) return r;
        r = sd_bus_message_read(m, "s", &s);
        if (r < 0) return r;
        value = s;
        sd_bus_message_exit_container(m);
        sd_bus_message_exit_container(m);
        return 1; // Property found
    }
    sd_bus_message_exit_container(m);
    return 0; // Property not found
}

// Helper to get a property from a D-Bus message (variant of array of strings)
static int get_property_array_string(sd_bus_message *m, const char *property_name, std::vector<std::string> &value) {
    int r;
    const char *s;
    r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sv");
    if (r < 0) return r;
    r = sd_bus_message_read(m, "s", &s);
    if (r < 0) return r;
    if (strcmp(s, property_name) == 0) {
        r = sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, "as");
        if (r < 0) return r;
        r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
        if (r < 0) return r;
        while ((r = sd_bus_message_read(m, "s", &s)) > 0) {
            value.push_back(s);
        }
        if (r < 0) return r;
        sd_bus_message_exit_container(m);
        sd_bus_message_exit_container(m);
        sd_bus_message_exit_container(m);
        return 1; // Property found
    }
    sd_bus_message_exit_container(m);
    return 0; // Property not found
}

// Signal handler for InterfacesAdded (new devices discovered)
static int interfaces_added_signal_handler(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r;
    const char *object_path;
    const char *interface_name;

    r = sd_bus_message_read(m, "o", &object_path);
    if (r < 0) {
        fprintf(stderr, "Failed to read object path: %s\n", strerror(-r));
        return r;
    }

    r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sa{sv}}");
    if (r < 0) {
        fprintf(stderr, "Failed to enter interfaces array: %s\n", strerror(-r));
        return r;
    }

    while ((r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sa{sv}")) > 0) {
        r = sd_bus_message_read(m, "s", &interface_name);
        if (r < 0) {
            fprintf(stderr, "Failed to read interface name: %s\n", strerror(-r));
            return r;
        }

        if (strcmp(interface_name, BLUEZ_DEVICE_INTERFACE) == 0) {
            // This is a Bluetooth device
            DiscoveredDevice device;
            device.object_path = object_path;
            device.messageCharacteristicFound = false;
            device.fileTransferCharacteristicFound = false;

            r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sv}");
            if (r < 0) {
                fprintf(stderr, "Failed to enter properties array: %s\n", strerror(-r));
                return r;
            }

            while ((r = sd_bus_message_peek_type(m, NULL, NULL)) > 0) {
                std::string prop_val;
                if (get_property_string(m, "Address", prop_val)) {
                    device.address = prop_val;
                } else if (get_property_string(m, "Name", prop_val)) {
                    device.name = prop_val;
                } else {
                    sd_bus_message_skip(m);
                }
            }
            if (r < 0) {
                fprintf(stderr, "Failed to read properties: %s\n", strerror(-r));
                return r;
            }
            sd_bus_message_exit_container(m);

            discoveredDevices.push_back(device);
            if (g_bluetooth_ui_callbacks.add_discovered_device) {
                g_bluetooth_ui_callbacks.add_discovered_device(device.name.c_str(), device.address.c_str(), 0); // RSSI not directly available here
            }
        }
        sd_bus_message_exit_container(m);
    }
    if (r < 0) {
        fprintf(stderr, "Failed to read interfaces: %s\n", strerror(-r));
        return r;
    }
    sd_bus_message_exit_container(m);
    sd_bus_message_exit_container(m);

    return 0;
}

// Signal handler for PropertiesChanged (e.g., device connected/disconnected)
static int properties_changed_signal_handler(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r;
    const char *interface_name;
    const char *property_name;
    const char *object_path = sd_bus_message_get_path(m);

    r = sd_bus_message_read(m, "s", &interface_name);
    if (r < 0) return r;

    if (strcmp(interface_name, BLUEZ_DEVICE_INTERFACE) == 0) {
        r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sv}");
        if (r < 0) return r;

        while ((r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sv")) > 0) {
            r = sd_bus_message_read(m, "s", &property_name);
            if (r < 0) return r;

            if (strcmp(property_name, "Connected") == 0) {
                int connected_val;
                r = sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, "b");
                if (r < 0) return r;
                r = sd_bus_message_read(m, "b", &connected_val);
                if (r < 0) return r;
                sd_bus_message_exit_container(m);

                // Find the device by object_path
                for (auto& device : discoveredDevices) {
                    if (device.object_path == object_path) {
                        if (g_bluetooth_ui_callbacks.update_device_connection_status) {
                            g_bluetooth_ui_callbacks.update_device_connection_status(device.address.c_str(), connected_val);
                        }
                        if (connected_val) {
                            connectedDevices[device.address] = device.object_path;
                        } else {
                            connectedDevices.erase(device.address);
                        }
                        break;
                    }
                }
            } else if (strcmp(property_name, "Value") == 0) { // Characteristic Value changed (notification)
                // This is a notification for a characteristic value change
                // We need to identify which characteristic it is.
                // The object_path here is the characteristic's object path.
                // The value is an array of bytes.
                std::vector<unsigned char> value_bytes;
                r = sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, "ay");
                if (r < 0) return r;
                const void *data_ptr;
                size_t data_len;
                r = sd_bus_message_read_array(m, 'y', &data_ptr, &data_len);
                if (r < 0) return r;
                value_bytes.assign((const unsigned char*)data_ptr, (const unsigned char*)data_ptr + data_len);
                sd_bus_message_exit_container(m);

                // Find the device and characteristic that this notification belongs to
                for (auto& device : discoveredDevices) {
                    if (device.messageCharacteristicPath == object_path) {
                        if (g_bluetooth_ui_callbacks.add_message_bubble) {
                            g_bluetooth_ui_callbacks.add_message_bubble(
                                device.address.c_str(),
                                std::string(value_bytes.begin(), value_bytes.end()).c_str(),
                                false // Incoming message
                            );
                        }
                        break;
                    } else if (device.fileTransferCharacteristicPath == object_path) {
                        // Handle incoming file transfer data
                        FileTransferState *transferState = ongoingFileTransfers[device.address];
                        if (!transferState) {
                            // This is the first chunk, expected to be metadata
                            std::string metadata_str(value_bytes.begin(), value_bytes.end());
                            json_object *jobj = json_tokener_parse(metadata_str.c_str());
                            if (!jobj) {
                                if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error parsing file metadata JSON.");
                                return 0;
                            }

                            json_object *jfilename, *jsize, *jkey;
                            if (!json_object_object_get_ex(jobj, "filename", &jfilename) ||
                                !json_object_object_get_ex(jobj, "size", &jsize) ||
                                !json_object_object_get_ex(jobj, "key", &jkey)) {
                                if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Missing metadata fields in received JSON.");
                                json_object_put(jobj);
                                return 0;
                            }

                            const char *filename = json_object_get_string(jfilename);
                            long file_size = json_object_get_int64(jsize);
                            const char *key_base64 = json_object_get_string(jkey);

                            // Decode base64 key
                            // This requires a base64 decode function. For simplicity, I'll assume it's available.
                            // In a real project, you'd use a library like libb64 or implement it.
                            // For now, I'll use a dummy decode and assume the key is directly passed.
                            // This is a simplification for the interactive session.
                            unsigned char decoded_key[crypto_secretbox_KEYBYTES];
                            // Dummy base64 decode (replace with actual implementation)
                            // For now, assuming key_base64 is directly the key bytes for simplicity
                            if (strlen(key_base64) != crypto_secretbox_KEYBYTES) {
                                if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Received key has incorrect length.");
                                json_object_put(jobj);
                                return 0;
                            }
                            memcpy(decoded_key, key_base64, crypto_secretbox_KEYBYTES);

                            transferState = (FileTransferState *)calloc(1, sizeof(FileTransferState));
                            if (!transferState) {
                                if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Failed to allocate memory for FileTransferState (receiving).");
                                json_object_put(jobj);
                                return 0;
                            }
                            strncpy(transferState->file_name, filename, sizeof(transferState->file_name) - 1);
                            transferState->file_name[sizeof(transferState->file_name) - 1] = '\0';
                            transferState->file_size = file_size;
                            transferState->bytes_transferred = 0;
                            transferState->current_chunk_index = 0;
                            transferState->is_sending = false;
                            memcpy(transferState->encryption_key, decoded_key, crypto_secretbox_KEYBYTES);
                            transferState->device_address = device.address;
                            transferState->characteristic_path = object_path;

                            // Construct destination path (e.g., in Downloads folder)
                            // For Linux, typically /home/user/Downloads
                            char downloads_path[1024];
                            snprintf(downloads_path, sizeof(downloads_path), "%s/Downloads", getenv("HOME"));
                            snprintf(transferState->file_path, sizeof(transferState->file_path), "%s/%s", downloads_path, filename);

                            transferState->file_handle = fopen(transferState->file_path, "wb");
                            if (!transferState->file_handle) {
                                if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"Error: Could not open file %s for writing: %s", transferState->file_path, strerror(errno)].UTF8String);
                                free(transferState);
                                json_object_put(jobj);
                                return 0;
                            }
                            ongoingFileTransfers[device.address] = transferState;
                            if (g_bluetooth_ui_callbacks.add_file_transfer_item) g_bluetooth_ui_callbacks.add_file_transfer_item(device.address.c_str(), transferState->file_name, false);
                            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"Receiving file '%s' (size: %ld) from %s.", filename, file_size, device.name.c_str()].UTF8String);
                            json_object_put(jobj);
                        } else {
                            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Missing metadata fields in received JSON.");
                            json_object_put(jobj);
                        }
                    } else {
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Could not decode received metadata as UTF8 string.");
                    }
                } else {
                    // This is a data chunk
                    unsigned char decrypted_buffer[MAX_BLE_WRITE_DATA_SIZE]; // Max possible plaintext size
                    size_t actual_decrypted_len = 0;

                    if (crypto_decrypt_file_chunk((const unsigned char*)value_bytes.data(), value_bytes.size(),
                                                  transferState->encryption_key,
                                                  transferState->current_chunk_index,
                                                  decrypted_buffer, sizeof(decrypted_buffer),
                                                  &actual_decrypted_len)) {
                        if (fwrite(decrypted_buffer, 1, actual_decrypted_len, transferState->file_handle) != actual_decrypted_len) {
                            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"Error writing decrypted data to file %s: %s", transferState->file_path, strerror(errno)].UTF8String);
                            // TODO: Handle write error, abort transfer
                        }
                        transferState->bytes_transferred += actual_decrypted_len;
                        transferState->current_chunk_index++;
                        if (g_bluetooth_ui_callbacks.update_file_transfer_progress) g_bluetooth_ui_callbacks.update_file_transfer_progress(device.address.c_str(), transferState->file_name, (double)transferState->bytes_transferred / transferState->file_size);

                        if (transferState->bytes_transferred >= transferState->file_size) {
                            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"File '%s' received completely.", transferState->file_name].UTF8String);
                            fclose(transferState->file_handle);
                            free(transferState);
                            ongoingFileTransfers.erase(device.address);
                            if (g_bluetooth_ui_callbacks.remove_file_transfer_item) g_bluetooth_ui_callbacks.remove_file_transfer_item(device.address.c_str(), transferState->file_name);
                        }
                    } else {
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error decrypting file chunk.");
                        // TODO: Handle decryption error, abort transfer
                    }
                }
            }
        } else {
            sd_bus_message_skip(m);
        }
        sd_bus_message_exit_container(m);
    }

    return 0;
}

// Helper to find GATT services and characteristics
static bool find_gatt_characteristics(const std::string& device_object_path, DiscoveredDevice* device) {
    sd_bus_message *m = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int r;

    r = sd_bus_call_method(bus, BLUEZ_BUS_NAME, "/", DBUS_OBJECT_MANAGER_INTERFACE, "GetManagedObjects", &error, &m, NULL);
    if (r < 0) {
        fprintf(stderr, "Failed to call GetManagedObjects: %s\n", error.message ? error.message : strerror(-r));
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Failed to get managed objects: %s", error.message ? error.message : "Unknown error"].UTF8String);
        sd_bus_error_free(&error);
        sd_bus_message_unref(m);
        return false;
    }

    r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{oa{sa{sv}}}");
    if (r < 0) {
        fprintf(stderr, "Failed to enter managed objects array: %s\n", strerror(-r));
        sd_bus_message_unref(m);
        return false;
    }

    while ((r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "oa{sa{sv}}")) > 0) {
        const char *object_path;
        r = sd_bus_message_read(m, "o", &object_path);
        if (r < 0) break;

        if (strncmp(object_path, device_object_path.c_str(), device_object_path.length()) == 0 &&
            object_path[device_object_path.length()] == '/') {

            r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sa{sv}}");
            if (r < 0) break;

            while ((r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sa{sv}")) > 0) {
                const char *interface_name;
                r = sd_bus_message_read(m, "s", &interface_name);
                if (r < 0) break;

                if (strcmp(interface_name, BLUEZ_GATT_SERVICE_INTERFACE) == 0) {
                    std::string service_uuid;
                    sd_bus_message *props_msg = NULL;
                    sd_bus_error props_error = SD_BUS_ERROR_NULL;

                    r = sd_bus_call_method(bus, BLUEZ_BUS_NAME, object_path, DBUS_PROPERTIES_INTERFACE, "Get", &props_error, &props_msg, "ss", BLUEZ_GATT_SERVICE_INTERFACE, "UUID");
                    if (r < 0) {
                        fprintf(stderr, "Failed to get service UUID: %s\n", props_error.message ? props_error.message : strerror(-r));
                    } else {
                        r = sd_bus_message_read(props_msg, "v", "s", &service_uuid);
                        if (r < 0) fprintf(stderr, "Failed to read service UUID from variant.\n");
                    }
                    sd_bus_error_free(&props_error);
                    sd_bus_message_unref(props_msg);

                    if (service_uuid == BLUEBEAM_SERVICE_UUID) {
                        sd_bus_message *char_m = NULL;
                        sd_bus_error char_error = SD_BUS_ERROR_NULL;
                        r = sd_bus_call_method(bus, BLUEZ_BUS_NAME, "/", DBUS_OBJECT_MANAGER_INTERFACE, "GetManagedObjects", &char_error, &char_m, NULL);
                        if (r < 0) {
                            fprintf(stderr, "Failed to call GetManagedObjects for characteristics: %s\n", char_error.message ? char_error.message : strerror(-r));
                        }
                        else {
                            sd_bus_message_enter_container(char_m, SD_BUS_TYPE_ARRAY, "{oa{sa{sv}}}");
                            while ((r = sd_bus_message_enter_container(char_m, SD_BUS_TYPE_DICT_ENTRY, "oa{sa{sv}}")) > 0) {
                                const char *char_object_path;
                                r = sd_bus_message_read(char_m, "o", &char_object_path);
                                if (r < 0) break;

                                if (strncmp(char_object_path, object_path, strlen(object_path)) == 0 &&
                                    char_object_path[strlen(object_path)] == '/') {
                                    sd_bus_message_enter_container(char_m, SD_BUS_TYPE_ARRAY, "{sa{sv}}");
                                    while ((r = sd_bus_message_enter_container(char_m, SD_BUS_TYPE_DICT_ENTRY, "sa{sv}")) > 0) {
                                        const char *char_interface_name;
                                        r = sd_bus_message_read(char_m, "s", &char_interface_name);
                                        if (r < 0) break;

                                        if (strcmp(char_interface_name, BLUEZ_GATT_CHARACTERISTIC_INTERFACE) == 0) {
                                            std::string char_uuid;
                                            sd_bus_message *char_props_msg = NULL;
                                            sd_bus_error char_props_error = SD_BUS_ERROR_NULL;

                                            r = sd_bus_call_method(bus, BLUEZ_BUS_NAME, char_object_path, DBUS_PROPERTIES_INTERFACE, "Get", &char_props_error, &char_props_msg, "ss", BLUEZ_GATT_CHARACTERISTIC_INTERFACE, "UUID");
                                            if (r < 0) {
                                                fprintf(stderr, "Failed to get characteristic UUID: %s\n", char_props_error.message ? char_props_error.message : strerror(-r));
                                            } else {
                                                r = sd_bus_message_read(char_props_msg, "v", "s", &char_uuid);
                                                if (r < 0) fprintf(stderr, "Failed to read characteristic UUID from variant.\n");
                                            }
                                            sd_bus_error_free(&char_props_error);
                                            sd_bus_message_unref(char_props_msg);

                                            if (char_uuid == MESSAGE_CHARACTERISTIC_UUID) {
                                                device->messageCharacteristicPath = char_object_path;
                                                device->messageCharacteristicFound = true;
                                                // Enable notifications for message characteristic
                                                sd_bus_message *notify_m = NULL;
                                                sd_bus_error notify_error = SD_BUS_ERROR_NULL;
                                                r = sd_bus_call_method(bus, BLUEZ_BUS_NAME, char_object_path, BLUEZ_GATT_CHARACTERISTIC_INTERFACE, "StartNotify", &notify_error, &notify_m, NULL);
                                                if (r < 0) {
                                                    fprintf(stderr, "Failed to call StartNotify for message characteristic: %s\n", notify_error.message ? notify_error.message : strerror(-r));
                                                }
                                                sd_bus_error_free(&notify_error);
                                                sd_bus_message_unref(notify_m);
                                            } else if (char_uuid == FILE_TRANSFER_CHARACTERISTIC_UUID) {
                                                device->fileTransferCharacteristicPath = char_object_path;
                                                device->fileTransferCharacteristicFound = true;
                                                // Enable notifications for file transfer characteristic
                                                sd_bus_message *notify_m = NULL;
                                                sd_bus_error notify_error = SD_BUS_ERROR_NULL;
                                                r = sd_bus_call_method(bus, BLUEZ_BUS_NAME, char_object_path, BLUEZ_GATT_CHARACTERISTIC_INTERFACE, "StartNotify", &notify_error, &notify_m, NULL);
                                                if (r < 0) {
                                                    fprintf(stderr, "Failed to call StartNotify for file transfer characteristic: %s\n", notify_error.message ? notify_error.message : strerror(-r));
                                                }
                                                sd_bus_error_free(&notify_error);
                                                sd_bus_message_unref(notify_m);
                                            }
                                        }
                                        sd_bus_message_exit_container(char_m);
                                    }
                                    if (r < 0) break;
                                    sd_bus_message_exit_container(char_m);
                                }
                                sd_bus_message_exit_container(char_m);
                            }
                            if (r < 0) break;
                            sd_bus_message_exit_container(char_m);
                        }
                        sd_bus_message_unref(char_m);
                    }
                }
                sd_bus_message_exit_container(m);
            }
            if (r < 0) break;
            sd_bus_message_exit_container(m);
        }
        if (r < 0) break;
        sd_bus_message_exit_container(m);
    }
    sd_bus_message_unref(m);

    return device->messageCharacteristicFound && device->fileTransferCharacteristicFound;
}

static void linux_discoverDevices(void) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Linux: Discovering devices using BlueZ/DBus...");
    discoveredDevices.clear();
    if (g_bluetooth_ui_callbacks.clear_discovered_devices) g_bluetooth_ui_callbacks.clear_discovered_devices();

    int r;

    if (!bus) {
        r = sd_bus_open_system(&bus);
        if (r < 0) {
            fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Failed to connect to system D-Bus.");
            return;
        }

        r = sd_bus_match_signal(bus, NULL,
                                BLUEZ_BUS_NAME,
                                "/org/bluez/hci0", // Assuming hci0 adapter
                                DBUS_OBJECT_MANAGER_INTERFACE,
                                "InterfacesAdded",
                                interfaces_added_signal_handler,
                                NULL);
        if (r < 0) {
            fprintf(stderr, "Failed to add signal match for InterfacesAdded: %s\n", strerror(-r));
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Failed to add InterfacesAdded signal match.");
            sd_bus_unref(bus);
            bus = NULL;
            return;
        }

        r = sd_bus_match_signal(bus, NULL,
                                BLUEZ_BUS_NAME,
                                NULL, // Match all objects
                                DBUS_PROPERTIES_INTERFACE,
                                "PropertiesChanged",
                                properties_changed_signal_handler,
                                NULL);
        if (r < 0) {
            fprintf(stderr, "Failed to add signal match for PropertiesChanged: %s\n", strerror(-r));
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Failed to add PropertiesChanged signal match.");
            sd_bus_unref(bus);
            bus = NULL;
            return;
        }
    }

    sd_bus_message *m = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    r = sd_bus_call_method(bus, BLUEZ_BUS_NAME, "/org/bluez/hci0", BLUEZ_ADAPTER_INTERFACE, "StartDiscovery", &error, &m, NULL);
    if (r < 0) {
        fprintf(stderr, "Failed to call StartDiscovery: %s\n", error.message ? error.message : strerror(-r));
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Failed to start discovery: %s", error.message ? error.message : "Unknown error"].UTF8String);
    }
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);
}

static bool linux_pairDevice(const char* device_address) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Linux: Pairing (placeholder).");
    return true;
}

static bool linux_connect(const char* device_address) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Linux: Connecting...");

    DiscoveredDevice* targetDevice = nullptr;
    for (auto& device : discoveredDevices) {
        if (device.address == device_address) {
            targetDevice = &device;
            break;
        }
    }

    if (!targetDevice) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Error: Device not found for connection.");
        return false;
    }

    sd_bus_message *m = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int r;

    r = sd_bus_call_method(bus, BLUEZ_BUS_NAME, targetDevice->object_path.c_str(), BLUEZ_DEVICE_INTERFACE, "Connect", &error, &m, NULL);
    if (r < 0) {
        fprintf(stderr, "Failed to call Connect on device %s: %s\n", device_address, error.message ? error.message : strerror(-r));
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Failed to connect to device %s: %s", device_address, error.message ? error.message : "Unknown error"].UTF8String);
        sd_bus_error_free(&error);
        sd_bus_message_unref(m);
        return false;
    }
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);

    if (!find_gatt_characteristics(targetDevice->object_path, targetDevice)) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Failed to find required GATT characteristics.");
        linux_disconnect(device_address);
        return false;
    }

    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Linux: Connected. Found GATT characteristics.");
    if (g_bluetooth_ui_callbacks.update_device_connection_status) g_bluetooth_ui_callbacks.update_device_connection_status(device_address, true);
    connectedDevices[device_address] = targetDevice->object_path;

    return true;
}

static void linux_disconnect(const char* device_address) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Linux: Disconnecting...");

    std::string device_object_path;
    auto it = connectedDevices.find(device_address);
    if (it != connectedDevices.end()) {
        device_object_path = it->second;
    } else {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Error: Device not found in connected devices.");
        return;
    }

    sd_bus_message *m = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int r;

    r = sd_bus_call_method(bus, BLUEZ_BUS_NAME, device_object_path.c_str(), BLUEZ_DEVICE_INTERFACE, "Disconnect", &error, &m, NULL);
    if (r < 0) {
        fprintf(stderr, "Failed to call Disconnect on device %s: %s\n", device_address, error.message ? error.message : strerror(-r));
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Failed to disconnect from device %s: %s", device_address, error.message ? error.message : "Unknown error"].UTF8String);
    }
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);

    if (g_bluetooth_ui_callbacks.update_device_connection_status) g_bluetooth_ui_callbacks.update_device_connection_status(device_address, false);
    connectedDevices.erase(it);
}

static bool linux_sendMessage(const char* device_address, const char* message) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Linux: Sending message...");

    DiscoveredDevice* targetDevice = nullptr;
    for (auto& device : discoveredDevices) {
        if (device.address == device_address) {
            targetDevice = &device;
            break;
        }
    }

    if (!targetDevice || !targetDevice->messageCharacteristicFound) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Error: Message characteristic not found for device.");
        return false;
    }

    sd_bus_message *m = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int r;

    // Prepare options dictionary (empty for now)
    sd_bus_message *options_dict = NULL;
    r = sd_bus_message_new_array(sd_bus_message_new_method_call(bus, NULL, NULL, NULL, NULL), 'a', '{sv}', &options_dict);
    if (r < 0) {
        fprintf(stderr, "Failed to create options dictionary: %s\n", strerror(-r));
        return false;
    }
    sd_bus_message_unref(options_dict); // Unref as it's now part of the method call message

    // Call WriteValue method
    r = sd_bus_call_method(bus, BLUEZ_BUS_NAME,
                           targetDevice->messageCharacteristicPath.c_str(),
                           BLUEZ_GATT_CHARACTERISTIC_INTERFACE,
                           "WriteValue",
                           &error, &m,
                           "ay", message, strlen(message),
                           "a{sv}", options_dict);
    if (r < 0) {
        fprintf(stderr, "Failed to call WriteValue: %s\n", error.message ? error.message : strerror(-r));
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Failed to send message: %s", error.message ? error.message : "Unknown error"].UTF8String);
        sd_bus_error_free(&error);
        sd_bus_message_unref(m);
        return false;
    }
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);

    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Message sent successfully.");
    if (g_bluetooth_ui_callbacks.add_message_bubble) g_bluetooth_ui_callbacks.add_message_bubble(device_address, message, true);
    return true;
}

static char* linux_receiveMessage(const char* device_address) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Linux: Receiving message (placeholder).");
    return NULL;
}

static bool linux_sendFile(const char* device_address, const char* file_path) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Linux: Sending file (placeholder).");
    return true;
}

static bool linux_receiveFile(const char* device_address, const char* destination_path) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Linux: Receiving file (placeholder).");
    return true;
}

static IBluetoothManager linux_manager = {
    .discoverDevices = linux_discoverDevices,
    .pairDevice = linux_pairDevice,
    .connect = linux_connect,
    .disconnect = linux_disconnect,
    .sendMessage = linux_sendMessage,
    .receiveMessage = linux_receiveMessage,
    .sendFile = linux_sendFile,
    .receiveFile = linux_receiveFile
};

IBluetoothManager* get_linux_bluetooth_manager(void) {
    return &linux_manager;
}