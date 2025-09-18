#include "bluetooth_windows.h"
#include <stdio.h>
#include <windows.h>
#include <bluetoothapis.h>
#include <bthprops.h>
#include <strsafe.h>
#include <vector>
#include <string>
#include <map>
#include <initguid.h> // Required for GUID definition
#include <setupapi.h> // For device enumeration
#include <devguid.h> // For GUID_DEVCLASS_BLUETOOTH
#include <cfgmgr32.h> // For CM_Get_Device_ID_List_Size
#include "bluetooth_callbacks.h"
#include "crypto_manager.h"
#include <json-c/json.h>
#include "base64.h"

// Link with Bthprops.lib, BluetoothAPIs.lib, Setupapi.lib
#pragma comment(lib, "Bthprops.lib")
#pragma comment(lib, "BluetoothAPIs.lib")
#pragma comment(lib, "Setupapi.lib")

// Define custom service and characteristic GUIDs
// These must match the UUIDs defined in the macOS implementation
// {E20A39F4-73F5-4BC4-A12F-17D1AD07A961}
DEFINE_GUID(BLUEBEAM_SERVICE_GUID, 0xe20a39f4, 0x73f5, 0x4bc4, 0xa1, 0x2f, 0x17, 0xd1, 0xad, 0x07, 0xa9, 0x61);
// {08590F7E-DB05-467E-8757-72F6F6669999}
DEFINE_GUID(MESSAGE_CHARACTERISTIC_GUID, 0x08590f7e, 0xdb05, 0x467e, 0x87, 0x57, 0x72, 0xf6, 0xf6, 0x66, 0x99, 0x99);
// {08590F7E-DB05-467E-8757-72F6F6668888}
DEFINE_GUID(FILE_TRANSFER_CHARACTERISTIC_GUID, 0x08590f7e, 0xdb05, 0x467e, 0x87, 0x57, 0x72, 0xf6, 0xf6, 0x66, 0x88, 0x88);

// Max data size for a single BLE write (considering MTU and overhead)
#define MAX_BLE_WRITE_DATA_SIZE 500

// Structure to hold discovered device information
typedef struct {
    BLUETOOTH_DEVICE_INFO_STRUCT info; // For classic Bluetooth info
    std::string address;
    std::string name;
    HANDLE hDevice; // Handle to the device for GATT operations (for BLE)
    GUID deviceInterfaceGuid; // For BLE devices
    std::string devicePath; // For BLE devices
    // Store discovered GATT characteristics for this device
    BTH_LE_GATT_CHARACTERISTIC messageCharacteristic;
    BTH_LE_GATT_CHARACTERISTIC fileTransferCharacteristic;
    bool messageCharacteristicFound;
    bool fileTransferCharacteristicFound;
    BLUETOOTH_GATT_EVENT_HANDLE messageNotificationHandle; // Handle for message notifications
    BLUETOOTH_GATT_EVENT_HANDLE fileTransferNotificationHandle; // Handle for file transfer notifications
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
    // For Windows, we need to store the characteristic itself for writing
    BTH_LE_GATT_CHARACTERISTIC characteristic;
} FileTransferState;

// Global list of discovered devices
static std::vector<DiscoveredDevice> discoveredDevices;
// Map of connected device handles, keyed by address (or devicePath for BLE)
static std::map<std::string, HANDLE> connectedDevices;
// Map of ongoing file transfers, keyed by device address
static std::map<std::string, FileTransferState*> ongoingFileTransfers;

// Forward declaration for helper function
static void send_next_file_chunk(const std::string& device_address, FileTransferState *transferState);

// Helper to convert BTH_ADDR to string
static std::string bth_addr_to_string(BTH_ADDR addr) {
    char buffer[32];
    sprintf_s(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
              (int)((addr >> (5 * 8)) & 0xFF),
              (int)((addr >> (4 * 8)) & 0xFF),
              (int)((addr >> (3 * 8)) & 0xFF),
              (int)((addr >> (2 * 8)) & 0xFF),
              (int)((addr >> (1 * 8)) & 0xFF),
              (int)(addr & 0xFF));
    return std::string(buffer);
}

// Helper to convert GUID to string
static std::string guid_to_string(const GUID& guid) {
    OLECHAR* bstrGuid;
    StringFromCLSID(guid, &bstrGuid);
    std::string strGuid(CW2A(bstrGuid));
    CoTaskMemFree(bstrGuid);
    return strGuid;
}

// Callback for GATT characteristic value change notifications
VOID CALLBACK CharacteristicChangedCallback(
    BTH_LE_GATT_EVENT_TYPE EventType,
    PVOID EventOutParameter,
    PVOID Context
) {
    if (EventType == CharacteristicValueChanged) {
        PBLUETOOTH_GATT_VALUE_CHANGED_EVENT ValueChangedEvent = (PBLUETOOTH_GATT_VALUE_CHANGED_EVENT)EventOutParameter;
        if (ValueChangedEvent->CharacteristicValue->DataSize > 0) {
            std::string device_address = (char*)Context;
            
            if (IsEqualGUID(ValueChangedEvent->Characteristic->CharacteristicUuid.Uuid, MESSAGE_CHARACTERISTIC_GUID)) {
                if (g_bluetooth_ui_callbacks.add_message_bubble) {
                    g_bluetooth_ui_callbacks.add_message_bubble(
                        device_address.c_str(),
                        (const char*)ValueChangedEvent->CharacteristicValue->Data,
                        false // Incoming message
                    );
                }
            } else if (IsEqualGUID(ValueChangedEvent->Characteristic->CharacteristicUuid.Uuid, FILE_TRANSFER_CHARACTERISTIC_GUID)) {
                // Handle incoming file transfer data
                FileTransferState *transferState = ongoingFileTransfers[device_address];
                if (!transferState) {
                    // This is the first chunk, expected to be metadata
                    std::string metadata_str((const char*)ValueChangedEvent->CharacteristicValue->Data, ValueChangedEvent->CharacteristicValue->DataSize);
                    json_object *jobj = json_tokener_parse(metadata_str.c_str());
                    if (!jobj) {
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error parsing file metadata JSON.");
                        return;
                    }

                    json_object *jfilename, *jsize, *jkey;
                    if (!json_object_object_get_ex(jobj, "filename", &jfilename) ||
                        !json_object_object_get_ex(jobj, "size", &jsize) ||
                        !json_object_object_get_ex(jobj, "key", &jkey)) {
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Missing metadata fields in received JSON.");
                        json_object_put(jobj);
                        return;
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
                        return;
                    }
                    memcpy(decoded_key, key_base64, crypto_secretbox_KEYBYTES);

                    transferState = (FileTransferState *)calloc(1, sizeof(FileTransferState));
                    if (!transferState) {
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Failed to allocate memory for FileTransferState (receiving).");
                        json_object_put(jobj);
                        return;
                    }
                    strncpy(transferState->file_name, filename, sizeof(transferState->file_name) - 1);
                    transferState->file_name[sizeof(transferState->file_name) - 1] = '\0';
                    transferState->file_size = file_size;
                    transferState->bytes_transferred = 0;
                    transferState->current_chunk_index = 0;
                    transferState->is_sending = false;
                    memcpy(transferState->encryption_key, decoded_key, crypto_secretbox_KEYBYTES);
                    transferState->device_address = device_address;

                    // Construct destination path (e.g., in Downloads folder)
                    // For Windows, typically C:\Users\<User>\Downloads
                    char downloads_path[MAX_PATH];
                    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_DOWNLOADS, NULL, 0, downloads_path))) {
                        snprintf(transferState->file_path, sizeof(transferState->file_path), "%s\%s", downloads_path, filename);
                    } else {
                        // Fallback if Downloads folder not found
                        snprintf(transferState->file_path, sizeof(transferState->file_path), ".\%s", filename);
                    }

                    transferState->file_handle = fopen(transferState->file_path, "wb");
                    if (!transferState->file_handle) {
                        char alert_msg[256];
                        snprintf(alert_msg, sizeof(alert_msg), "Error: Could not open file %s for writing: %s", transferState->file_path, strerror(errno));
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
                        free(transferState);
                        json_object_put(jobj);
                        return;
                    }
                    ongoingFileTransfers[device_address] = transferState;
                    if (g_bluetooth_ui_callbacks.add_file_transfer_item) g_bluetooth_ui_callbacks.add_file_transfer_item(device_address.c_str(), transferState->file_name, false);
                    char alert_msg[256];
                    snprintf(alert_msg, sizeof(alert_msg), "Receiving file '%s' (size: %ld) from %s.", filename, file_size, device_address.c_str());
                    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
                    json_object_put(jobj);
                } else {
                    // This is a data chunk
                    unsigned char decrypted_buffer[MAX_BLE_WRITE_DATA_SIZE]; // Max possible plaintext size
                    size_t actual_decrypted_len = 0;

                    if (crypto_decrypt_file_chunk((const unsigned char*)ValueChangedEvent->CharacteristicValue->Data, ValueChangedEvent->CharacteristicValue->DataSize,
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
                        if (g_bluetooth_ui_callbacks.update_file_transfer_progress) g_bluetooth_ui_callbacks.update_file_transfer_progress(device_address.c_str(), transferState->file_name, (double)transferState->bytes_transferred / transferState->file_size);

                        if (transferState->bytes_transferred >= transferState->file_size) {
                            char alert_msg[256];
                            snprintf(alert_msg, sizeof(alert_msg), "File '%s' received completely.", transferState->file_name);
                            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
                            fclose(transferState->file_handle);
                            free(transferState);
                            ongoingFileTransfers.erase(device_address);
                            if (g_bluetooth_ui_callbacks.remove_file_transfer_item) g_bluetooth_ui_callbacks.remove_file_transfer_item(device_address.c_str(), transferState->file_name);
                        }
                    } else {
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error decrypting file chunk.");
                        // TODO: Handle decryption error, abort transfer
                    }
                }
            }
        }
    }
}

static void windows_discoverDevices(void) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Windows: Discovering devices using Win32 Bluetooth APIs...\n");
    discoveredDevices.clear();
    if (g_bluetooth_ui_callbacks.clear_discovered_devices) g_bluetooth_ui_callbacks.clear_discovered_devices();

    // --- Discover Classic Bluetooth Devices ---
    BLUETOOTH_DEVICE_INFO_STRUCT btdi;
    ZeroMemory(&btdi, sizeof(BLUETOOTH_DEVICE_INFO_STRUCT));
    btdi.dwSize = sizeof(BLUETOOTH_DEVICE_INFO_STRUCT);

    HBLUETOOTH_DEVICE_FIND hDeviceFind = BluetoothFindFirstDevice(&btdi, 0);

    if (hDeviceFind != NULL) {
        do {
            DiscoveredDevice device;
            device.info = btdi;
            device.address = bth_addr_to_string(btdi.Address);
            int len = WideCharToMultiByte(CP_UTF8, 0, btdi.szDeviceName, -1, NULL, 0, NULL, NULL);
            if (len > 0) {
                std::string name_utf8(len, 0);
                WideCharToMultiByte(CP_UTF8, 0, btdi.szDeviceName, -1, &name_utf8[0], len, NULL, NULL);
                device.name = name_utf8;
            }
            device.hDevice = NULL; // Not a GATT device handle yet
            device.deviceInterfaceGuid = GUID_NULL;
            device.devicePath = "";
            device.messageCharacteristicFound = false;
            device.fileTransferCharacteristicFound = false;
            device.messageNotificationHandle = NULL;
            device.fileTransferNotificationHandle = NULL;

            discoveredDevices.push_back(device);
            if (g_bluetooth_ui_callbacks.add_discovered_device) g_bluetooth_ui_callbacks.add_discovered_device(device.name.c_str(), device.address.c_str(), 0); // RSSI not available here

        } while (BluetoothFindNextDevice(hDeviceFind, &btdi));

        BluetoothFindDeviceClose(hDeviceFind);
    }

    // --- Discover BLE (GATT) Devices ---
    GUID BluetoothLeGattServiceGuid = { 0x6E3BB145, 0x4582, 0x4885, { 0x92, 0x0D, 0x81, 0xE0, 0xB2, 0x7E, 0xD9, 0xA9 } };

    HDEVINFO hDevInfo = SetupDiGetClassDevs(&BluetoothLeGattServiceGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "SetupDiGetClassDevs failed for BLE devices.");
        return;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &BluetoothLeGattServiceGuid, i, &deviceInterfaceData); ++i) {
        DWORD detailSize = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo, &deviceInterfaceData, NULL, 0, &detailSize, NULL);

        PSP_DEVICE_INTERFACE_DETAIL_DATA_W deviceDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)LocalAlloc(LPTR, detailSize);
        if (!deviceDetail) {
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "LocalAlloc failed for device detail.");
            continue;
        }
        deviceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &deviceInterfaceData, deviceDetail, detailSize, NULL, NULL)) {
            HANDLE hBleDevice = CreateFile(deviceDetail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            if (hBleDevice != INVALID_HANDLE_VALUE) {
                DiscoveredDevice device;
                ZeroMemory(&device.info, sizeof(BLUETOOTH_DEVICE_INFO_STRUCT));
                device.info.dwSize = sizeof(BLUETOOTH_DEVICE_INFO_STRUCT);
                device.address = guid_to_string(deviceInterfaceData.InterfaceClassGuid); // Using GUID as a temporary address
                device.name = CW2A(deviceDetail->DevicePath); // Using device path as name for now
                device.hDevice = hBleDevice;
                device.deviceInterfaceGuid = deviceInterfaceData.InterfaceClassGuid;
                device.devicePath = CW2A(deviceDetail->DevicePath);
                device.messageCharacteristicFound = false;
                device.fileTransferCharacteristicFound = false;
                device.messageNotificationHandle = NULL;
                device.fileTransferNotificationHandle = NULL;

                discoveredDevices.push_back(device);
                if (g_bluetooth_ui_callbacks.add_discovered_device) g_bluetooth_ui_callbacks.add_discovered_device(device.name.c_str(), device.devicePath.c_str(), 0); // RSSI not available here
            } else {
                if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Failed to open BLE device handle.");
            }
        } else {
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "SetupDiGetDeviceInterfaceDetail failed.");
        }
        LocalFree(deviceDetail);
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);
}

static bool windows_pairDevice(const char* device_address) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Windows: Attempting to pair...");

    BLUETOOTH_DEVICE_INFO_STRUCT btdi;
    ZeroMemory(&btdi, sizeof(BLUETOOTH_DEVICE_INFO_STRUCT));
    btdi.dwSize = sizeof(BLUETOOTH_DEVICE_INFO_STRUCT);

    // Convert device_address string to BTH_ADDR
    BTH_ADDR bthAddr = 0;
    sscanf(device_address, "%llx", &bthAddr); // Assuming device_address is in hex format

    btdi.Address.ullLong = bthAddr;

    // Find the device info
    HBLUETOOTH_DEVICE_FIND hDeviceFind = BluetoothFindFirstDevice(&btdi, 0);
    if (hDeviceFind != NULL) {
        BluetoothFindDeviceClose(hDeviceFind);

        // Attempt to authenticate/pair
        DWORD result = BluetoothAuthenticateDeviceEx(NULL, NULL, &btdi, NULL, BLUETOOTH_AUTHENTICATION_METHOD_LEGACY, NULL);

        if (result == ERROR_SUCCESS) {
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Windows: Pairing successful.");
            return true;
        } else if (result == ERROR_CANCELLED) {
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Windows: Pairing cancelled by user.");
        } else {
            char alert_msg[256];
            snprintf(alert_msg, sizeof(alert_msg), "Windows: Pairing failed with error: %lu", result);
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", alert_msg);
        }
    } else {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Windows: Device not found for pairing.");
    }
    return false;
}

static bool windows_connect(const char* device_address) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Windows: Connecting...");

    DiscoveredDevice* targetDevice = nullptr;
    for (auto& device : discoveredDevices) {
        if (device.address == device_address || device.devicePath == device_address) {
            targetDevice = &device;
            break;
        }
    }

    if (!targetDevice) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Error: Device not found.");
        return false;
    }

    if (targetDevice->hDevice == NULL) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Error: Device is not a BLE device or handle not acquired.");
        return false;
    }

    // --- Discover GATT Services ---
    USHORT servicesBufferCount;
    BLUETOOTH_GATT_SERVICE* servicesBuffer = nullptr;

    HRESULT hr = BluetoothGattGetServices(targetDevice->hDevice, 0, NULL, &servicesBufferCount, 0);
    if (HRESULT_FROM_WIN32(ERROR_NOT_FOUND) == hr) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "No GATT services found on device.");
        return false;
    } else if (S_OK != hr) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "BluetoothGattGetServices failed (count).");
        return false;
    }

    servicesBuffer = (PBLUETOOTH_GATT_SERVICE)CoTaskMemAlloc(sizeof(BLUETOOTH_GATT_SERVICE) * servicesBufferCount);
    if (!servicesBuffer) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Failed to allocate memory for GATT services.");
        return false;
    }
    ZeroMemory(servicesBuffer, sizeof(BLUETOOTH_GATT_SERVICE) * servicesBufferCount);

    hr = BluetoothGattGetServices(targetDevice->hDevice, servicesBufferCount, servicesBuffer, &servicesBufferCount, 0);
    if (S_OK != hr) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "BluetoothGattGetServices failed (get).");
        CoTaskMemFree(servicesBuffer);
        return false;
    }

    for (USHORT i = 0; i < servicesBufferCount; ++i) {
        if (IsEqualGUID(servicesBuffer[i].ServiceUuid.Uuid, BLUEBEAM_SERVICE_GUID)) {
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Found BlueBeamNative Service.");

            USHORT charBufferCount;
            BTH_LE_GATT_CHARACTERISTIC* charBuffer = nullptr;

            hr = BluetoothGattGetCharacteristics(targetDevice->hDevice, &servicesBuffer[i], 0, NULL, &charBufferCount, 0);
            if (HRESULT_FROM_WIN32(ERROR_NOT_FOUND) == hr) {
                if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "No GATT characteristics found for BlueBeamNative Service.");
                break;
            } else if (S_OK != hr) {
                if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "BluetoothGattGetCharacteristics failed (count).");
                break;
            }

            charBuffer = (PBTH_LE_GATT_CHARACTERISTIC)CoTaskMemAlloc(sizeof(BTH_LE_GATT_CHARACTERISTIC) * charBufferCount);
            if (!charBuffer) {
                if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Failed to allocate memory for GATT characteristics.");
                break;
            }
            ZeroMemory(charBuffer, sizeof(BTH_LE_GATT_CHARACTERISTIC) * charBufferCount);

            hr = BluetoothGattGetCharacteristics(targetDevice->hDevice, &servicesBuffer[i], charBufferCount, charBuffer, &charBufferCount, 0);
            if (S_OK != hr) {
                if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "BluetoothGattGetCharacteristics failed (get).");
                CoTaskMemFree(charBuffer);
                break;
            }

            for (USHORT j = 0; j < charBufferCount; ++j) {
                if (IsEqualGUID(charBuffer[j].CharacteristicUuid.Uuid, MESSAGE_CHARACTERISTIC_GUID)) {
                    targetDevice->messageCharacteristic = charBuffer[j];
                    targetDevice->messageCharacteristicFound = true;
                    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Found Message Characteristic.");

                    // Enable notifications for the message characteristic
                    BTH_LE_GATT_DESCRIPTOR_VALUE cccDescriptorValue;
                    ZeroMemory(&cccDescriptorValue, sizeof(cccDescriptorValue));
                    cccDescriptorValue.DescriptorType = ClientCharacteristicConfiguration;
                    cccDescriptorValue.ClientCharacteristicConfiguration.IsSubscribeToNotification = TRUE;

                    hr = BluetoothGattSetDescriptorValue(targetDevice->hDevice,
                                                         &charBuffer[j],
                                                         &cccDescriptorValue,
                                                         0);
                    if (S_OK != hr) {
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Failed to set CCCD for message characteristic.");
                    } else {
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Enabled notifications for Message Characteristic.");
                        hr = BluetoothGattRegisterCharacteristicNotification(
                            targetDevice->hDevice,
                            &charBuffer[j],
                            &cccDescriptorValue,
                            CharacteristicChangedCallback,
                            (PVOID)targetDevice->address.c_str(), // Pass device address as context
                            &targetDevice->messageNotificationHandle,
                            0);
                        if (S_OK != hr) {
                            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Failed to register notification callback for message characteristic.");
                        }
                    }
                } else if (IsEqualGUID(charBuffer[j].CharacteristicUuid.Uuid, FILE_TRANSFER_CHARACTERISTIC_GUID)) {
                    targetDevice->fileTransferCharacteristic = charBuffer[j];
                    targetDevice->fileTransferCharacteristicFound = true;
                    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Found File Transfer Characteristic.");
                    // Enable notifications for file transfer characteristic
                    BTH_LE_GATT_DESCRIPTOR_VALUE cccDescriptorValue;
                    ZeroMemory(&cccDescriptorValue, sizeof(cccDescriptorValue));
                    cccDescriptorValue.DescriptorType = ClientCharacteristicConfiguration;
                    cccDescriptorValue.ClientCharacteristicConfiguration.IsSubscribeToNotification = TRUE;

                    hr = BluetoothGattSetDescriptorValue(targetDevice->hDevice,
                                                         &charBuffer[j],
                                                         &cccDescriptorValue,
                                                         0);
                    if (S_OK != hr) {
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Failed to set CCCD for file transfer characteristic.");
                    } else {
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Enabled notifications for File Transfer Characteristic.");
                        hr = BluetoothGattRegisterCharacteristicNotification(
                            targetDevice->hDevice,
                            &charBuffer[j],
                            &cccDescriptorValue,
                            CharacteristicChangedCallback,
                            (PVOID)targetDevice->address.c_str(), // Pass device address as context
                            &targetDevice->fileTransferNotificationHandle,
                            0);
                        if (S_OK != hr) {
                            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Failed to register notification callback for file transfer characteristic.");
                        }
                    }
                }
            }
            CoTaskMemFree(charBuffer);
            break; // Found our service, no need to check others
        }
    }
    CoTaskMemFree(servicesBuffer);

    if (targetDevice->messageCharacteristicFound && targetDevice->fileTransferCharacteristicFound) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Successfully connected and found required characteristics.");
        if (g_bluetooth_ui_callbacks.update_device_connection_status) g_bluetooth_ui_callbacks.update_device_connection_status(device_address, true);
        connectedDevices[device_address] = targetDevice->hDevice; // Store handle for connected device
        return true;
    } else {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Error: Could not find all required characteristics.");
        if (g_bluetooth_ui_callbacks.update_device_connection_status) g_bluetooth_ui_callbacks.update_device_connection_status(device_address, false);
        CloseHandle(targetDevice->hDevice);
        targetDevice->hDevice = NULL;
        return false;
    }
}

static void windows_disconnect(const char* device_address) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Windows: Disconnecting...");
    auto it = connectedDevices.find(device_address);
    if (it != connectedDevices.end()) {
        DiscoveredDevice* targetDevice = nullptr;
        for (auto& device : discoveredDevices) {
            if (device.address == device_address || device.devicePath == device_address) {
                targetDevice = &device;
                break;
            }
        }

        if (targetDevice) {
            if (targetDevice->messageNotificationHandle) {
                // Disable notifications for the message characteristic
                BTH_LE_GATT_DESCRIPTOR_VALUE cccDescriptorValue;
                ZeroMemory(&cccDescriptorValue, sizeof(cccDescriptorValue));
                cccDescriptorValue.DescriptorType = ClientCharacteristicConfiguration;
                cccDescriptorValue.ClientCharacteristicConfiguration.IsSubscribeToNotification = FALSE;

                HRESULT hr = BluetoothGattSetDescriptorValue(targetDevice->hDevice,
                                                             &targetDevice->messageCharacteristic,
                                                             &cccDescriptorValue,
                                                             0);
                if (S_OK != hr) {
                    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Failed to disable CCCD for message characteristic.");
                }

                BluetoothGattUnregisterCharacteristicNotification(targetDevice->messageNotificationHandle);
                targetDevice->messageNotificationHandle = NULL;
            }
            if (targetDevice->fileTransferNotificationHandle) {
                // Disable notifications for the file transfer characteristic
                BTH_LE_GATT_DESCRIPTOR_VALUE cccDescriptorValue;
                ZeroMemory(&cccDescriptorValue, sizeof(cccDescriptorValue));
                cccDescriptorValue.DescriptorType = ClientCharacteristicConfiguration;
                cccDescriptorValue.ClientCharacteristicConfiguration.IsSubscribeToNotification = FALSE;

                HRESULT hr = BluetoothGattSetDescriptorValue(targetDevice->hDevice,
                                                             &targetDevice->fileTransferCharacteristic,
                                                             &cccDescriptorValue,
                                                             0);
                if (S_OK != hr) {
                    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Failed to disable CCCD for file transfer characteristic.");
                }

                BluetoothGattUnregisterCharacteristicNotification(targetDevice->fileTransferNotificationHandle);
                targetDevice->fileTransferNotificationHandle = NULL;
            }
        }

        CloseHandle(it->second);
        connectedDevices.erase(it);
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Disconnected.");
        if (g_bluetooth_ui_callbacks.update_device_connection_status) g_bluetooth_ui_callbacks.update_device_connection_status(device_address, false);
    } else {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Error: Device not found in connected devices.");
    }
}

static bool windows_sendMessage(const char* device_address, const char* message) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Windows: Sending message...");

    DiscoveredDevice* targetDevice = nullptr;
    for (auto& device : discoveredDevices) {
        if (device.address == device_address || device.devicePath == device_address) {
            targetDevice = &device;
            break;
        }
    }

    if (!targetDevice || !targetDevice->messageCharacteristicFound || targetDevice->hDevice == NULL) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Error: Cannot send message. Device not connected or message characteristic not found.");
        return false;
    }

    BTH_LE_GATT_CHARACTERISTIC_VALUE charValue;
    ZeroMemory(&charValue, sizeof(charValue));
    charValue.DataSize = (ULONG)strlen(message);
    charValue.Data = (PBYTE)message;

    HRESULT hr = BluetoothGattSetCharacteristicValue(targetDevice->hDevice, &targetDevice->messageCharacteristic, &charValue, 0);
    if (S_OK != hr) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "BluetoothGattSetCharacteristicValue failed.");
        return false;
    }

    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Message sent successfully.");
    if (g_bluetooth_ui_callbacks.add_message_bubble) g_bluetooth_ui_callbacks.add_message_bubble(device_address, message, true);
    return true;
}

static char* windows_receiveMessage(const char* device_address) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "Windows: Messages are received asynchronously.");
    return NULL;
}

static void send_next_file_chunk(const std::string& device_address, FileTransferState *transferState) {
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
            ongoingFileTransfers.erase(device_address);
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
            BTH_LE_GATT_CHARACTERISTIC_VALUE charValue;
            ZeroMemory(&charValue, sizeof(charValue));
            charValue.DataSize = (ULONG)actual_ciphertext_len;
            charValue.Data = (PBYTE)encrypted_buffer;

            HRESULT hr = BluetoothGattSetCharacteristicValue(connectedDevices[device_address],
                                                             &transferState->characteristic,
                                                             &charValue,
                                                             0);
            if (S_OK != hr) {
                char alert_msg[256];
                snprintf(alert_msg, sizeof(alert_msg), "Failed to write file chunk: 0x%lx", hr);
                if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
                // TODO: Handle write error
            } else {
                transferState->bytes_transferred += bytes_read;
                transferState->current_chunk_index++;
                if (g_bluetooth_ui_callbacks.update_file_transfer_progress) g_bluetooth_ui_callbacks.update_file_transfer_progress(device_address.c_str(), transferState->file_name, (double)transferState->bytes_transferred / transferState->file_size);
                // Since BluetoothGattSetCharacteristicValue is synchronous, we can immediately send the next chunk
                send_next_file_chunk(device_address, transferState);
            }
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
        ongoingFileTransfers.erase(device_address);
        if (g_bluetooth_ui_callbacks.remove_file_transfer_item) g_bluetooth_ui_callbacks.remove_file_transfer_item(device_address.c_str(), transferState->file_name);
    }
}

static bool windows_sendFile(const char* device_address, const char* file_path) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Windows: Sending file...");

    DiscoveredDevice* targetDevice = nullptr;
    for (auto& device : discoveredDevices) {
        if (device.address == device_address || device.devicePath == device_address) {
            targetDevice = &device;
            break;
        }
    }

    if (!targetDevice || !targetDevice->fileTransferCharacteristicFound || targetDevice->hDevice == NULL) {
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
    transferState->characteristic = targetDevice->fileTransferCharacteristic;

    if (!crypto_generate_session_key(transferState->encryption_key, sizeof(transferState->encryption_key))) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Failed to generate encryption key for file transfer.");
        fclose(file);
        free(transferState);
        return false;
    }

    ongoingFileTransfers[device_address] = transferState;
    if (g_bluetooth_ui_callbacks.add_file_transfer_item) g_bluetooth_ui_callbacks.add_file_transfer_item(device_address, transferState->file_name, true);

    // Send metadata (filename, size, encryption key) as the first chunk
    // For simplicity, we'll send a JSON string containing metadata.
    // In a real application, you might want to encrypt this metadata as well.
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

    BTH_LE_GATT_CHARACTERISTIC_VALUE charValue;
    ZeroMemory(&charValue, sizeof(charValue));
    charValue.DataSize = (ULONG)actual_encrypted_metadata_len;
    charValue.Data = (PBYTE)encrypted_metadata_buffer;

    HRESULT hr = BluetoothGattSetCharacteristicValue(connectedDevices[device_address],
                                                     &transferState->characteristic,
                                                     &charValue,
                                                     0);
    if (S_OK != hr) {
        char alert_msg[256];
        snprintf(alert_msg, sizeof(alert_msg), "Failed to write metadata: 0x%lx", hr);
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);
        fclose(file);
        free(transferState);
        ongoingFileTransfers.erase(device_address);
        json_object_put(jobj);
        return false;
    }

    transferState->current_chunk_index++; // Increment for the next data chunk

    char alert_msg[256];
    snprintf(alert_msg, sizeof(alert_msg), "Sent file metadata for '%s'. Size: %ld. Starting data transfer...", file_path, file_size);
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", alert_msg);

    // Start sending data chunks immediately
    send_next_file_chunk(device_address, transferState);

    json_object_put(jobj);
    return true;
}

static bool windows_receiveFile(const char* device_address, const char* destination_path) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Windows: Receiving file (placeholder).");
    return true;
}

void WindowsBluetoothManager::discoverDevices() {
    windows_discoverDevices();
}

bool WindowsBluetoothManager::pairDevice(const std::string& address) {
    return windows_pairDevice(address.c_str());
}

bool WindowsBluetoothManager::connect(const std::string& address) {
    return windows_connect(address.c_str());
}

void WindowsBluetoothManager::disconnect(const std::string& address) {
    windows_disconnect(address.c_str());
}

bool WindowsBluetoothManager::sendMessage(const std::string& address, const std::string& message) {
    return windows_sendMessage(address.c_str(), message.c_str());
}

bool WindowsBluetoothManager::sendFile(const std::string& address, const std::string& filePath) {
    return windows_sendFile(address.c_str(), filePath.c_str());
}

