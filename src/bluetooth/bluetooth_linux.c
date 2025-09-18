#include "bluetooth_linux.h"
#include <stdio.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <vector>
#include <map>
#include "crypto_manager.h"
#include <json-c/json.h>
#include "base64.h"

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

#define MAX_BLE_WRITE_DATA_SIZE 500

typedef struct {
    std::string object_path;
    std::string address;
    std::string name;
    std::string messageCharacteristicPath;
    std::string fileTransferCharacteristicPath;
    bool messageCharacteristicFound;
    bool fileTransferCharacteristicFound;
} DiscoveredDevice;

typedef struct {
    FILE *file_handle;
    char file_path[1024];
    char file_name[256];
    long file_size;
    long bytes_transferred;
    unsigned long long current_chunk_index;
    unsigned char encryption_key[crypto_secretbox_KEYBYTES];
    bool is_sending;
    std::string device_address;
    std::string characteristic_path;
} FileTransferState;

static IBluetoothManagerEvents* s_listener = nullptr;
static sd_bus *bus = NULL;
static std::vector<DiscoveredDevice> discoveredDevices;
static std::map<std::string, std::string> connectedDevices;
static std::map<std::string, FileTransferState*> ongoingFileTransfers;

static void send_next_file_chunk(FileTransferState *transferState);

// ... (rest of the file is the same, just replace g_bluetooth_ui_callbacks with s_listener)

void LinuxBluetoothManager::setEventListener(IBluetoothManagerEvents* listener) {
    s_listener = listener;
}

// ... (rest of the implementation of LinuxBluetoothManager methods)

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
                            unsigned char* decoded_key = base64_decode(key_base64, strlen(key_base64), &decoded_key_len);
                            if (!decoded_key || decoded_key_len != crypto_secretbox_KEYBYTES) {
                                if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Received key has incorrect length or decoding failed.");
                                json_object_put(jobj);
                                if (decoded_key) free(decoded_key);
                                return 0;
                            }
                            memcpy(decoded_key, decoded_key, crypto_secretbox_KEYBYTES);

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
                                char alert_msg[256];
                                snprintf(alert_msg, sizeof(alert_msg), "Error: Could not open file %s for writing: %s", transferState->file_path, strerror(errno));
                                if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
                                free(transferState);
                                json_object_put(jobj);
                                return 0;
                            }
                            ongoingFileTransfers[device.address] = transferState;
                            if (g_bluetooth_ui_callbacks.add_file_transfer_item) g_bluetooth_ui_callbacks.add_file_transfer_item(device.address.c_str(), transferState->file_name, false);
                            char alert_msg[256];
                            snprintf(alert_msg, sizeof(alert_msg), "Receiving file '%s' (size: %ld) from %s.", filename, file_size, device.name.c_str());
                            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
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
                            char alert_msg[256];
                            snprintf(alert_msg, sizeof(alert_msg), "File '%s' received completely.", transferState->file_name);
                            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
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
        char alert_msg[256];
        snprintf(alert_msg, sizeof(alert_msg), "Failed to start discovery: %s", error.message ? error.message : "Unknown error");
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", alert_msg);
    }
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);
}

static bool linux_pairDevice(const char* device_address) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Linux: Attempting to pair...");

    DiscoveredDevice* targetDevice = nullptr;
    for (auto& device : discoveredDevices) {
        if (device.address == device_address) {
            targetDevice = &device;
            break;
        }
    }

    if (!targetDevice) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Error: Device not found for pairing.");
        return false;
    }

    sd_bus_message *m = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int r;

    r = sd_bus_call_method(bus, BLUEZ_BUS_NAME, targetDevice->object_path.c_str(), BLUEZ_DEVICE_INTERFACE, "Pair", &error, &m, NULL);
    if (r < 0) {
        fprintf(stderr, "Failed to call Pair on device %s: %s\n", device_address, error.message ? error.message : strerror(-r));
        char alert_msg[256];
        snprintf(alert_msg, sizeof(alert_msg), "Failed to pair with device %s: %s", device_address, error.message ? error.message : "Unknown error");
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", alert_msg);
        sd_bus_error_free(&error);
        sd_bus_message_unref(m);
        return false;
    }
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);

    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Linux: Pairing initiated. Check device for confirmation.");
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
        char alert_msg[256];
        snprintf(alert_msg, sizeof(alert_msg), "Failed to connect to device %s: %s", device_address, error.message ? error.message : "Unknown error");
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", alert_msg);
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
        char alert_msg[256];
        snprintf(alert_msg, sizeof(alert_msg), "Failed to disconnect from device %s: %s", device_address, error.message ? error.message : "Unknown error");
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", alert_msg);
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
        char alert_msg[256];
        snprintf(alert_msg, sizeof(alert_msg), "Failed to send message: %s", error.message ? error.message : "Unknown error");
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", alert_msg);
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
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Linux: Sending file...");

    DiscoveredDevice* targetDevice = nullptr;
    for (auto& device : discoveredDevices) {
        if (device.address == device_address) {
            targetDevice = &device;
            break;
        }
    }

    if (!targetDevice || !targetDevice->fileTransferCharacteristicFound) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Cannot send file. Device not connected or file transfer characteristic not found.");
        return false;
    }

    if (ongoingFileTransfers.count(device_address)) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: A file transfer is already in progress for this device.");
        return false;
    }

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        char alert_msg[256];
        snprintf(alert_msg, sizeof(alert_msg), "Error: Could not open file %s for reading: %s", file_path, strerror(errno));
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
        return false;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size == -1) {
        char alert_msg[256];
        snprintf(alert_msg, sizeof(alert_msg), "Error: Could not get file size for %s: %s", file_path, strerror(errno));
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
        fclose(file);
        return false;
    }

    FileTransferState *transferState = (FileTransferState *)calloc(1, sizeof(FileTransferState));
    if (!transferState) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Failed to allocate memory for FileTransferState.");
        fclose(file);
        return false;
    }
    transferState->file_handle = file;
    strncpy(transferState->file_path, file_path, sizeof(transferState->file_path) - 1);
    transferState->file_path[sizeof(transferState->file_path) - 1] = '\0';
    strncpy(transferState->file_name, strrchr(file_path, '/') ? strrchr(file_path, '/') + 1 : file_path, sizeof(transferState->file_name) - 1);
    transferState->file_name[sizeof(transferState->file_name) - 1] = '\0';
    transferState->file_size = file_size;
    transferState->bytes_transferred = 0;
    transferState->current_chunk_index = 0;
    transferState->is_sending = true;
    transferState->device_address = device_address;
    transferState->characteristic_path = targetDevice->fileTransferCharacteristicPath;

    if (!crypto_generate_session_key(transferState->encryption_key, sizeof(transferState->encryption_key))) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Failed to generate encryption key for file transfer.");
        fclose(file);
        free(transferState);
        return false;
    }

    ongoingFileTransfers[device_address] = transferState;
    if (g_bluetooth_ui_callbacks.add_file_transfer_item) g_bluetooth_ui_callbacks.add_file_transfer_item(device_address, transferState->file_name, true);

    // Send metadata (filename, size, encryption key) as the first chunk
    json_object *jobj = json_object_new_object();
    json_object_object_add(jobj, "filename", json_object_new_string(transferState->file_name));
    json_object_object_add(jobj, "size", json_object_new_int64(file_size));
    char* encoded_key = base64_encode(transferState->encryption_key, sizeof(transferState->encryption_key));
    json_object_object_add(jobj, "key", json_object_new_string(encoded_key));
    free(encoded_key); // Free the allocated base64 string

    const char *metadata_json_str = json_object_to_json_string(jobj);
    size_t metadata_len = strlen(metadata_json_str);

    unsigned char encrypted_metadata_buffer[metadata_len + crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES];
    size_t actual_encrypted_metadata_len = 0;

    if (!crypto_encrypt_file_chunk((const unsigned char*)metadata_json_str, metadata_len,
                                  transferState->encryption_key,
                                  transferState->current_chunk_index, // Chunk 0 for metadata
                                  encrypted_metadata_buffer, sizeof(encrypted_metadata_buffer),
                                  &actual_encrypted_metadata_len)) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Failed to encrypt file metadata.");
        fclose(file);
        free(transferState);
        ongoingFileTransfers.erase(device_address);
        json_object_put(jobj);
        return false;
    }

    sd_bus_message *m = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int r;

    // Prepare options dictionary (empty for now)
    sd_bus_message *options_dict = NULL;
    r = sd_bus_message_new_array(sd_bus_message_new_method_call(bus, NULL, NULL, NULL, NULL), 'a', '{sv}', &options_dict);
    if (r < 0) {
        fprintf(stderr, "Failed to create options dictionary for metadata: %s\n", strerror(-r));
        json_object_put(jobj);
        return false;
    }
    sd_bus_message_unref(options_dict); // Unref as it's now part of the method call message

    r = sd_bus_call_method(bus, BLUEZ_BUS_NAME,
                           targetDevice->fileTransferCharacteristicPath.c_str(),
                           BLUEZ_GATT_CHARACTERISTIC_INTERFACE,
                           "WriteValue",
                           &error, &m,
                           "ay", encrypted_metadata_buffer, actual_encrypted_metadata_len,
                           "a{sv}", options_dict);
    if (r < 0) {
        fprintf(stderr, "Failed to write metadata chunk: %s\n", error.message ? error.message : strerror(-r));
        char alert_msg[256];
        snprintf(alert_msg, sizeof(alert_msg), "Failed to write metadata: %s", error.message ? error.message : "Unknown error");
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
        fclose(file);
        free(transferState);
        ongoingFileTransfers.erase(device_address);
        json_object_put(jobj);
        return false;
    }
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);

    transferState->current_chunk_index++; // Increment for the next data chunk

    char alert_msg[256];
    snprintf(alert_msg, sizeof(alert_msg), "Sent file metadata for '%s'. Size: %ld. Starting data transfer...", file_path, file_size);
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);

    // Start sending data chunks immediately
    send_next_file_chunk(transferState);

    json_object_put(jobj);
    return true;
}

static void send_next_file_chunk(FileTransferState *transferState) {
    if (!transferState || !transferState->file_handle || transferState->bytes_transferred >= transferState->file_size) {
        if (transferState && transferState->file_handle) {
            fclose(transferState->file_handle);
            transferState->file_handle = NULL;
        }
        if (transferState) {
            char alert_msg[256];
            snprintf(alert_msg, sizeof(alert_msg), "File '%s' sent completely.", transferState->file_name);
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
            if (g_bluetooth_ui_callbacks.remove_file_transfer_item) g_bluetooth_ui_callbacks.remove_file_transfer_item(transferState->device_address.c_str(), transferState->file_name);
            free(transferState);
            ongoingFileTransfers.erase(transferState->device_address);
        }
        return;
    }

    unsigned char buffer[MAX_BLE_WRITE_DATA_SIZE];
    size_t bytes_to_read = MAX_BLE_WRITE_DATA_SIZE;
    if (transferState->bytes_transferred + bytes_to_read > transferState->file_size) {
        bytes_to_read = transferState->file_size - transferState->bytes_transferred;
    }

    size_t bytes_read = fread(buffer, 1, bytes_to_read, transferState->file_handle);

    if (bytes_read > 0) {
        unsigned char encrypted_buffer[MAX_BLE_WRITE_DATA_SIZE + crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES];
        size_t actual_ciphertext_len = 0;

        if (crypto_encrypt_file_chunk(buffer, bytes_read,
                                      transferState->encryption_key,
                                      transferState->current_chunk_index,
                                      encrypted_buffer, sizeof(encrypted_buffer),
                                      &actual_ciphertext_len)) {
            sd_bus_message *m = NULL;
            sd_bus_error error = SD_BUS_ERROR_NULL;
            int r;

            // Prepare options dictionary (empty for now)
            sd_bus_message *options_dict = NULL;
            r = sd_bus_message_new_array(sd_bus_message_new_method_call(bus, NULL, NULL, NULL, NULL), 'a', '{sv}', &options_dict);
            if (r < 0) {
                fprintf(stderr, "Failed to create options dictionary for chunk: %s\n", strerror(-r));
                return;
            }
            sd_bus_message_unref(options_dict); // Unref as it's now part of the method call message

            r = sd_bus_call_method(bus, BLUEZ_BUS_NAME,
                                   transferState->characteristic_path.c_str(),
                                   BLUEZ_GATT_CHARACTERISTIC_INTERFACE,
                                   "WriteValue",
                                   &error, &m,
                                   "ay", encrypted_buffer, actual_ciphertext_len,
                                   "a{sv}", options_dict);
            if (r < 0) {
                fprintf(stderr, "Failed to write data chunk: %s\n", error.message ? error.message : strerror(-r));
                char alert_msg[256];
                snprintf(alert_msg, sizeof(alert_msg), "Failed to write data chunk: %s", error.message ? error.message : "Unknown error");
                if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
                // TODO: Handle write error
            } else {
                transferState->bytes_transferred += bytes_read;
                transferState->current_chunk_index++;
                if (g_bluetooth_ui_callbacks.update_file_transfer_progress) g_bluetooth_ui_callbacks.update_file_transfer_progress(transferState->device_address.c_str(), transferState->file_name, (double)transferState->bytes_transferred / transferState->file_size);
                // Recursively call to send the next chunk
                send_next_file_chunk(transferState);
            }
            sd_bus_error_free(&error);
            sd_bus_message_unref(m);
        } else {
            char alert_msg[256];
            snprintf(alert_msg, sizeof(alert_msg), "Error encrypting file chunk for '%s'.", transferState->file_name);
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
            // TODO: Handle encryption error
        }
    } else if (ferror(transferState->file_handle)) {
        char alert_msg[256];
        snprintf(alert_msg, sizeof(alert_msg), "Error reading file '%s': %s", transferState->file_name, strerror(errno));
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
        // TODO: Handle file read error
    } else { // EOF reached, and all bytes sent
        char alert_msg[256];
        snprintf(alert_msg, sizeof(alert_msg), "File '%s' sent completely.", transferState->file_name);
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
        fclose(transferState->file_handle);
        transferState->file_handle = NULL;
        free(transferState);
        ongoingFileTransfers.erase(transferState->device_address);
        if (g_bluetooth_ui_callbacks.remove_file_transfer_item) g_bluetooth_ui_callbacks.remove_file_transfer_item(transferState->device_address.c_str(), transferState->file_name);
    }
}

static bool linux_receiveFile(const char* device_address, const char* destination_path) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Linux: Receiving file (placeholder).");
    return true;
}

void LinuxBluetoothManager::discoverDevices() {
    linux_discoverDevices();
}

bool LinuxBluetoothManager::pairDevice(const std::string& address) {
    return linux_pairDevice(address.c_str());
}

bool LinuxBluetoothManager::connect(const std::string& address) {
    return linux_connect(address.c_str());
}

void LinuxBluetoothManager::disconnect(const std::string& address) {
    linux_disconnect(address.c_str());
}

bool LinuxBluetoothManager::sendMessage(const std::string& address, const std::string& message) {
    return linux_sendMessage(address.c_str(), message.c_str());
}

bool LinuxBluetoothManager::sendFile(const std::string& address, const std::string& filePath) {
    return linux_sendFile(address.c_str(), filePath.c_str());
}
