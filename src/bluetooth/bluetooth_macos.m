#include "bluetooth_macos.h"
#include <stdio.h>
#include <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>
#include <string.h>
#include <errno.h>
#include "crypto_manager.h"
#include <sodium.h>

@interface BBFileTransferState : NSObject
{
    @public FileTransferState _cState; // Holds the C struct
}
@end

@implementation BBFileTransferState
// No custom implementation needed for now, just a wrapper
@end
#include "bluetooth_callbacks.h"

// Define custom service and characteristic UUIDs
// These are placeholder UUIDs. In a real application, you would generate your own unique UUIDs.
#define BLUEBEAM_SERVICE_UUID_STRING        @"E20A39F4-73F5-4BC4-A12F-17D1AD07A961"
#define MESSAGE_CHARACTERISTIC_UUID_STRING  @"08590F7E-DB05-467E-8757-72F6F6669999"
#define FILE_TRANSFER_CHARACTERISTIC_UUID_STRING @"08590F7E-DB05-467E-8757-72F6F6668888"

// Max data size for a single BLE write (considering MTU and overhead)
// This is a common value, but actual MTU can be negotiated.
#define MAX_BLE_WRITE_DATA_SIZE 500



// Global dictionaries to store transfer states, keyed by peripheral UUID string
static NSMutableDictionary<NSString *, BBFileTransferState *> *ongoingFileTransfers = nil;

// Define a global pointer to the CBCentralManager
static CBCentralManager *centralManager = nil;
// Array to store discovered peripherals
static NSMutableArray<CBPeripheral *> *discoveredPeripherals = nil;
// Dictionary to store connected peripherals by address (or UUID string)
static NSMutableDictionary<NSString *, CBPeripheral *> *connectedPeripherals = nil;

// Dictionary to store characteristics for connected peripherals
static NSMutableDictionary<NSString *, CBCharacteristic *> *messageCharacteristics = nil;
static NSMutableDictionary<NSString *, CBCharacteristic *> *fileTransferCharacteristics = nil;

// Forward declaration for helper function
static void send_next_file_chunk(CBPeripheral *peripheral, CBCharacteristic *characteristic, BBFileTransferState *transferState);

// Delegate for CBCentralManager
@interface BluetoothCentralManagerDelegate : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
@end

@implementation BluetoothCentralManagerDelegate

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    switch (central.state) {
        case CBManagerStatePoweredOn:
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "CoreBluetooth State: Powered On. Starting scan...");
            [discoveredPeripherals removeAllObjects];
            if (g_bluetooth_ui_callbacks.clear_discovered_devices) g_bluetooth_ui_callbacks.clear_discovered_devices();
            CBUUID *bluebeamServiceUUID = [CBUUID UUIDWithString:BLUEBEAM_SERVICE_UUID_STRING];
            [central scanForPeripheralsWithServices:@[bluebeamServiceUUID] options:@{CBCentralManagerScanOptionAllowDuplicatesKey : @YES}];
            break;
        case CBManagerStatePoweredOff:
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "CoreBluetooth State: Powered Off. Please turn on Bluetooth.");
            [central stopScan];
            [connectedPeripherals enumerateKeysAndObjectsUsingBlock:^(NSString *key, CBPeripheral *peripheral, BOOL *stop) {
                [central cancelPeripheralConnection:peripheral];
            }];
            break;
        case CBManagerStateResetting:
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "CoreBluetooth State: Resetting. Bluetooth is temporarily unavailable.");
            break;
        case CBManagerStateUnauthorized:
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "CoreBluetooth State: Unauthorized. Please grant Bluetooth permission to this application in System Settings > Privacy & Security > Bluetooth.");
            break;
        case CBManagerStateUnknown:
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "CoreBluetooth State: Unknown. Bluetooth state could not be determined.");
            break;
        case CBManagerStateUnsupported:
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "CoreBluetooth State: Unsupported. Bluetooth Low Energy is not supported on this device.");
            break;
        default:
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"CoreBluetooth State: Unhandled state (%ld).", (long)central.state].UTF8String);
            break;
    }
}

- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *,id> *)advertisementData RSSI:(NSNumber *)RSSI {
    if (![discoveredPeripherals containsObject:peripheral]) {
        [discoveredPeripherals addObject:peripheral];
        if (g_bluetooth_ui_callbacks.add_discovered_device) {
            g_bluetooth_ui_callbacks.add_discovered_device(
                peripheral.name.UTF8String ? peripheral.name.UTF8String : "(null)",
                peripheral.identifier.UUIDString.UTF8String,
                RSSI.intValue
            );
        }
    }
}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Connected to peripheral: %@", peripheral.name].UTF8String);
    if (g_bluetooth_ui_callbacks.update_device_connection_status) g_bluetooth_ui_callbacks.update_device_connection_status(peripheral.identifier.UUIDString.UTF8String, true);
    [connectedPeripherals setObject:peripheral forKey:peripheral.identifier.UUIDString];
    peripheral.delegate = self;
    CBUUID *bluebeamServiceUUID = [CBUUID UUIDWithString:BLUEBEAM_SERVICE_UUID_STRING];
    [peripheral discoverServices:@[bluebeamServiceUUID]];
}

- (void)centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Failed to connect to peripheral %@: %@", peripheral.name, error.localizedDescription].UTF8String);
    if (g_bluetooth_ui_callbacks.update_device_connection_status) g_bluetooth_ui_callbacks.update_device_connection_status(peripheral.identifier.UUIDString.UTF8String, false);
}

- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Disconnected from peripheral %@: %@", peripheral.name, error ? error.localizedDescription : @"(null)"].UTF8String);
    if (g_bluetooth_ui_callbacks.update_device_connection_status) g_bluetooth_ui_callbacks.update_device_connection_status(peripheral.identifier.UUIDString.UTF8String, false);
    [connectedPeripherals removeObjectForKey:peripheral.identifier.UUIDString];
    [messageCharacteristics removeObjectForKey:peripheral.identifier.UUIDString];
    [fileTransferCharacteristics removeObjectForKey:peripheral.identifier.UUIDString];
    
    BBFileTransferState *transferState = [ongoingFileTransfers objectForKey:peripheral.identifier.UUIDString];
    if (transferState) {
        if (transferState->_cState.file_handle) {
            fclose(transferState->_cState.file_handle);
        }
        free(transferState);
        [ongoingFileTransfers removeObjectForKey:peripheral.identifier.UUIDString];
    }

    if (error) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Disconnection error: %@", error.localizedDescription].UTF8String);
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error {
    if (error) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Error discovering services for %@: %@", peripheral.name, error.localizedDescription].UTF8String);
        return;
    }

    for (CBService *service in peripheral.services) {
        CBUUID *messageCharUUID = [CBUUID UUIDWithString:MESSAGE_CHARACTERISTIC_UUID_STRING];
        CBUUID *fileTransferCharUUID = [CBUUID UUIDWithString:FILE_TRANSFER_CHARACTERISTIC_UUID_STRING];
        [peripheral discoverCharacteristics:@[messageCharUUID, fileTransferCharUUID] forService:service];
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error {
    if (error) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Error discovering characteristics for service %@ of %@: %@", service.UUID.UUIDString, peripheral.name, error.localizedDescription].UTF8String);
        return;
    }

    for (CBCharacteristic *characteristic in service.characteristics) {
        if ([characteristic.UUID.UUIDString isEqualToString:MESSAGE_CHARACTERISTIC_UUID_STRING]) {
            [messageCharacteristics setObject:characteristic forKey:peripheral.identifier.UUIDString];
            [peripheral setNotifyValue:YES forCharacteristic:characteristic];
        } else if ([characteristic.UUID.UUIDString isEqualToString:FILE_TRANSFER_CHARACTERISTIC_UUID_STRING]) {
            [fileTransferCharacteristics setObject:characteristic forKey:peripheral.identifier.UUIDString];
            [peripheral setNotifyValue:YES forCharacteristic:characteristic];
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    if (error) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Error updating value for characteristic %@: %@", characteristic.UUID.UUIDString, error.localizedDescription].UTF8String);
        return;
    }

    if (characteristic.value) {
        if ([characteristic.UUID.UUIDString isEqualToString:MESSAGE_CHARACTERISTIC_UUID_STRING]) {
            if (g_bluetooth_ui_callbacks.add_message_bubble) {
                g_bluetooth_ui_callbacks.add_message_bubble(
                    peripheral.identifier.UUIDString.UTF8String,
                    [[NSString alloc] initWithData:characteristic.value encoding:NSUTF8StringEncoding].UTF8String,
                    false // Incoming message
                );
            }
        } else if ([characteristic.UUID.UUIDString isEqualToString:FILE_TRANSFER_CHARACTERISTIC_UUID_STRING]) {
            BBFileTransferState *transferState = [ongoingFileTransfers objectForKey:peripheral.identifier.UUIDString];
            if (!transferState) {
                // This is the first chunk, expected to be metadata
                NSString *metadataString = [[NSString alloc] initWithData:characteristic.value encoding:NSUTF8StringEncoding];
                if (metadataString) {
                    NSError *jsonError = nil;
                    NSDictionary *metadata = [NSJSONSerialization JSONObjectWithData:characteristic.value options:0 error:&jsonError];
                    if (jsonError) {
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"Error parsing file metadata JSON: %@", jsonError.localizedDescription].UTF8String);
                        return;
                    }

                    NSString *filename = metadata[@"filename"];
                    NSNumber *fileSize = metadata[@"size"];
                    NSString *keyBase64 = metadata[@"key"];

                    if (filename && fileSize && keyBase64) {
                        NSData *keyData = [[NSData alloc] initWithBase64EncodedString:keyBase64 options:0];
                        if (keyData.length != crypto_secretbox_KEYBYTES) {
                            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Received key has incorrect length.");
                            return;
                        }

                        BBFileTransferState *newTransferStateWrapper = [[BBFileTransferState alloc] init];
                        if (!newTransferStateWrapper) {
                            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Failed to allocate memory for BBFileTransferState (receiving).");
                            return;
                        }
                        newTransferStateWrapper->_cState = *(FileTransferState *)calloc(1, sizeof(FileTransferState));
                        transferState = newTransferStateWrapper;
                        if (!transferState->_cState.file_handle) { // Check the C struct's file_handle
                            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Failed to allocate memory for FileTransferState (receiving).");
                            return;
                        }
                        strncpy(transferState->_cState.file_name, filename.UTF8String, sizeof(transferState->_cState.file_name) - 1);
                        transferState->_cState.file_name[sizeof(transferState->_cState.file_name) - 1] = '\0';
                        transferState->_cState.file_size = fileSize.longValue;
                        transferState->_cState.bytes_transferred = 0;
                        transferState->_cState.current_chunk_index = 0;
                        transferState->_cState.is_sending = false;
                        memcpy(transferState->_cState.encryption_key, keyData.bytes, crypto_secretbox_KEYBYTES);

                        NSString *downloadsPath = NSSearchPathForDirectoriesInDomains(NSDownloadsDirectory, NSUserDomainMask, YES).firstObject;
                        NSString *destinationFilePath = [downloadsPath stringByAppendingPathComponent:filename];
                        strncpy(transferState->_cState.file_path, destinationFilePath.UTF8String, sizeof(transferState->_cState.file_path) - 1);
                        transferState->_cState.file_path[sizeof(transferState->_cState.file_path) - 1] = '\0';

                        transferState->_cState.file_handle = fopen(transferState->_cState.file_path, "wb");
                        if (!transferState->_cState.file_handle) {
                            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"Error: Could not open file %@ for writing: %s", destinationFilePath, strerror(errno)].UTF8String);
                            free(transferState); // Free the underlying C struct
                            return;
                        }
                        [ongoingFileTransfers setObject:transferState forKey:peripheral.identifier.UUIDString];
                        if (g_bluetooth_ui_callbacks.add_file_transfer_item) g_bluetooth_ui_callbacks.add_file_transfer_item(peripheral.identifier.UUIDString.UTF8String, transferState->_cState.file_name, false);
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"Receiving file '%@' (size: %ld) from %@.", filename, fileSize.longValue, peripheral.name].UTF8String);
                    } else {
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Missing metadata fields in received JSON.");
                    }
                } else {
                    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Could not decode received metadata as UTF8 string.");
                }
            } else {
                // This is a data chunk
                unsigned char decrypted_buffer[MAX_BLE_WRITE_DATA_SIZE];
                size_t actual_decrypted_len = 0;

                if (crypto_decrypt_file_chunk((const unsigned char*)characteristic.value.bytes, characteristic.value.length,
                                              transferState->_cState.encryption_key,
                                              transferState->_cState.current_chunk_index,
                                              decrypted_buffer, sizeof(decrypted_buffer),
                                              &actual_decrypted_len)) {
                    if (fwrite(decrypted_buffer, 1, actual_decrypted_len, transferState->_cState.file_handle) != actual_decrypted_len) {
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"Error writing decrypted data to file %s: %s", transferState->_cState.file_path, strerror(errno)].UTF8String);
                        // TODO: Handle write error, abort transfer
                    }
                    transferState->_cState.bytes_transferred += actual_decrypted_len;
                    transferState->_cState.current_chunk_index++;
                                        if (g_bluetooth_ui_callbacks.update_file_transfer_progress) g_bluetooth_ui_callbacks.update_file_transfer_progress(peripheral.identifier.UUIDString.UTF8String, transferState->_cState.file_name, (double)transferState->_cState.bytes_transferred / transferState->_cState.file_size);

                    if (transferState->_cState.bytes_transferred >= transferState->_cState.file_size) {
                        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"File '%@' received completely.", transferState->_cState.file_name].UTF8String);
                        fclose(transferState->_cState.file_handle);
                        // Free the C struct and remove the Objective-C wrapper
                        free(transferState); // Free the underlying C struct
                        [ongoingFileTransfers removeObjectForKey:peripheral.identifier.UUIDString];
                        if (g_bluetooth_ui_callbacks.remove_file_transfer_item) g_bluetooth_ui_callbacks.remove_file_transfer_item(peripheral.identifier.UUIDString.UTF8String, transferState->_cState.file_name);
                    }
                } else {
                    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error decrypting file chunk.");
                    // TODO: Handle decryption error, abort transfer
                }
            }
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didWriteValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    if (error) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Error writing value for characteristic %@: %@", characteristic.UUID.UUIDString, error.localizedDescription].UTF8String);
    } else {
        if ([characteristic.UUID.UUIDString isEqualToString:FILE_TRANSFER_CHARACTERISTIC_UUID_STRING]) {
            BBFileTransferState *transferState = [ongoingFileTransfers objectForKey:peripheral.identifier.UUIDString];
            if (transferState && transferState->_cState.is_sending) {
                send_next_file_chunk(peripheral, characteristic, transferState);
            }
        }
    }
}

@end

// Helper function to send file chunks
static void send_next_file_chunk(CBPeripheral *peripheral, CBCharacteristic *characteristic, BBFileTransferState *transferState) {
    if (!transferState || !transferState->_cState.file_handle || transferState->_cState.bytes_transferred >= transferState->_cState.file_size) {
        if (transferState && transferState->_cState.file_handle) {
            fclose(transferState->_cState.file_handle);
            transferState->_cState.file_handle = NULL;
        }
        if (transferState) {
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"File '%s' sent completely.", transferState->_cState.file_name].UTF8String);
            if (g_bluetooth_ui_callbacks.remove_file_transfer_item) g_bluetooth_ui_callbacks.remove_file_transfer_item(peripheral.identifier.UUIDString.UTF8String, transferState->_cState.file_name);
            // Free the C struct and remove the Objective-C wrapper
            free(transferState); // Free the underlying C struct
            [ongoingFileTransfers removeObjectForKey:peripheral.identifier.UUIDString];
        }
        return;
    }

    unsigned char buffer[MAX_BLE_WRITE_DATA_SIZE];
    size_t bytes_to_read = MAX_BLE_WRITE_DATA_SIZE;
    if (transferState->_cState.bytes_transferred + bytes_to_read > transferState->_cState.file_size) {
        bytes_to_read = transferState->_cState.file_size - transferState->_cState.bytes_transferred;
    }

    size_t bytes_read = fread(buffer, 1, bytes_to_read, transferState->_cState.file_handle);

    if (bytes_read > 0) {
        unsigned char encrypted_buffer[MAX_BLE_WRITE_DATA_SIZE + crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES];
        size_t actual_ciphertext_len = 0;

        if (crypto_encrypt_file_chunk(buffer, bytes_read,
                                      transferState->_cState.encryption_key,
                                      transferState->_cState.current_chunk_index,
                                      encrypted_buffer, sizeof(encrypted_buffer),
                                      &actual_ciphertext_len)) {
            NSData *dataToSend = [NSData dataWithBytes:encrypted_buffer length:actual_ciphertext_len];
            [peripheral writeValue:dataToSend forCharacteristic:characteristic type:CBCharacteristicWriteWithResponse];
            transferState->_cState.bytes_transferred += bytes_read;
            transferState->_cState.current_chunk_index++;
            if (g_bluetooth_ui_callbacks.update_file_transfer_progress) g_bluetooth_ui_callbacks.update_file_transfer_progress(peripheral.identifier.UUIDString.UTF8String, transferState->_cState.file_name, (double)transferState->_cState.bytes_transferred / transferState->_cState.file_size);
        } else {
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"Error encrypting file chunk for '%s'.", transferState->_cState.file_name].UTF8String);
            // TODO: Handle encryption error
        }
    } else if (ferror(transferState->_cState.file_handle)) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"Error reading file '%s': %s", transferState->_cState.file_name, strerror(errno)].UTF8String);
        // TODO: Handle file read error
    } else { // EOF reached, and all bytes sent
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"File '%s' completely sent.", transferState->_cState.file_name].UTF8String);
        fclose(transferState->_cState.file_handle);
        transferState->_cState.file_handle = NULL;
        // Free the C struct and remove the Objective-C wrapper
        free(transferState); // Free the underlying C struct
        [ongoingFileTransfers removeObjectForKey:peripheral.identifier.UUIDString];
        if (g_bluetooth_ui_callbacks.remove_file_transfer_item) g_bluetooth_ui_callbacks.remove_file_transfer_item(peripheral.identifier.UUIDString.UTF8String, transferState->_cState.file_name);
    }
}

// C functions for Bluetooth management
static void macos_discoverDevices(void) {
    if (!centralManager) {
        centralManager = [[CBCentralManager alloc] initWithDelegate:[[BluetoothCentralManagerDelegate alloc] init] queue:dispatch_get_main_queue()];
        discoveredPeripherals = [[NSMutableArray alloc] init];
        connectedPeripherals = [[NSMutableDictionary alloc] init];
        messageCharacteristics = [[NSMutableDictionary alloc] init];
        fileTransferCharacteristics = [[NSMutableDictionary alloc] init];
        ongoingFileTransfers = [[NSMutableDictionary alloc] init];
    } else {
        if (centralManager.state == CBManagerStatePoweredOn) {
            [discoveredPeripherals removeAllObjects];
            if (g_bluetooth_ui_callbacks.clear_discovered_devices) g_bluetooth_ui_callbacks.clear_discovered_devices();
            CBUUID *bluebeamServiceUUID = [CBUUID UUIDWithString:BLUEBEAM_SERVICE_UUID_STRING];
            [centralManager scanForPeripheralsWithServices:@[bluebeamServiceUUID] options:@{CBCentralManagerScanOptionAllowDuplicatesKey : @YES}];
        }
    }
}

static bool macos_pairDevice(const char* device_address) {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"macOS: Pairing with device %s (implicit).", device_address].UTF8String);
    return true;
}

static bool macos_connect(const char* device_address) {
    CBPeripheral *targetPeripheral = nil;
    for (CBPeripheral *peripheral in discoveredPeripherals) {
        if ([peripheral.identifier.UUIDString isEqualToString:[NSString stringWithUTF8String:device_address]]) {
            targetPeripheral = peripheral;
            break;
        }
    }

    if (targetPeripheral) {
        [centralManager stopScan];
        [centralManager connectPeripheral:targetPeripheral options:nil];
        return true;
    } else {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Error: Peripheral with address %s not found.", device_address].UTF8String);
        return false;
    }
}

static void macos_disconnect(const char* device_address) {
    CBPeripheral *targetPeripheral = [connectedPeripherals objectForKey:[NSString stringWithUTF8String:device_address]];
    if (targetPeripheral) {
        [centralManager cancelPeripheralConnection:targetPeripheral];
    } else {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Error: Peripheral with address %s not connected.", device_address].UTF8String);
    }
}

static bool macos_sendMessage(const char* device_address, const char* message) {
    CBPeripheral *peripheral = [connectedPeripherals objectForKey:[NSString stringWithUTF8String:device_address]];
    CBCharacteristic *characteristic = [messageCharacteristics objectForKey:[NSString stringWithUTF8String:device_address]];

    if (peripheral && characteristic) {
        NSData *data = [NSData dataWithBytes:message length:strlen(message)];
        [peripheral writeValue:data forCharacteristic:characteristic type:CBCharacteristicWriteWithResponse];
        if (g_bluetooth_ui_callbacks.add_message_bubble) g_bluetooth_ui_callbacks.add_message_bubble(device_address, message, true);
        return true;
    } else {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Error: Cannot send message. Peripheral not connected or message characteristic not found for %s.", device_address].UTF8String);
        return false;
    }
}

static char* macos_receiveMessage(const char* device_address) {
    return NULL;
}

static bool macos_sendFile(const char* device_address, const char* file_path) {
    CBPeripheral *peripheral = [connectedPeripherals objectForKey:[NSString stringWithUTF8String:device_address]];
    CBCharacteristic *characteristic = [fileTransferCharacteristics objectForKey:[NSString stringWithUTF8String:device_address]];

    if (!peripheral || !characteristic) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"Error: Cannot send file. Peripheral not connected or file transfer characteristic not found for %s.", device_address].UTF8String);
        return false;
    }

    if ([ongoingFileTransfers objectForKey:peripheral.identifier.UUIDString]) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"Error: A file transfer is already in progress for %s.", device_address].UTF8String);
        return false;
    }

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"Error: Could not open file %s for reading: %s", file_path, strerror(errno)].UTF8String);
        return false;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size == -1) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"Error: Could not get file size for %s: %s", file_path, strerror(errno)].UTF8String);
        fclose(file);
        return false;
    }

    BBFileTransferState *transferStateWrapper = [[BBFileTransferState alloc] init];
    if (!transferStateWrapper) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Failed to allocate memory for BBFileTransferState (sending).");
        fclose(file);
        return false;
    }
    transferStateWrapper->_cState = *(FileTransferState *)calloc(1, sizeof(FileTransferState));
    FileTransferState *transferState = &transferStateWrapper->_cState; // Get a pointer to the C struct
    if (!transferState) { // Check if the C struct allocation failed
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Failed to allocate memory for FileTransferState (sending).");
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

    if (!crypto_generate_session_key(transferState->encryption_key, sizeof(transferState->encryption_key))) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Failed to generate encryption key for file transfer.");
        fclose(file);
        free(transferState); // Free the underlying C struct
        return false;
    }

    [ongoingFileTransfers setObject:transferState forKey:peripheral.identifier.UUIDString];
    if (g_bluetooth_ui_callbacks.add_file_transfer_item) g_bluetooth_ui_callbacks.add_file_transfer_item(device_address, transferState->file_name, true);

    NSString *metadataString = [NSString stringWithFormat:@"{\"filename\":\"%s\", \"size\":%ld, \"key\":\"%@\"}",
                                                        transferState->file_name,
                                                        file_size,
                                                        [[NSData dataWithBytes:transferState->encryption_key length:sizeof(transferState->encryption_key)] base64EncodedStringWithOptions:0]];

    NSData *metadataData = [metadataString dataUsingEncoding:NSUTF8StringEncoding];

    unsigned char encrypted_metadata_buffer[metadataData.length + crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES];
    size_t actual_encrypted_metadata_len = 0;

    if (!crypto_encrypt_file_chunk((const unsigned char*)metadataData.bytes, metadataData.length,
                                  transferState->encryption_key,
                                  transferState->current_chunk_index, // Chunk 0 for metadata
                                  encrypted_metadata_buffer, sizeof(encrypted_metadata_buffer),
                                  &actual_encrypted_metadata_len)) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", "Error: Failed to encrypt file metadata.");
        fclose(file);
        free(transferState); // Free the underlying C struct
        [ongoingFileTransfers removeObjectForKey:peripheral.identifier.UUIDString];
        return false;
    }

    NSData *dataToSend = [NSData dataWithBytes:encrypted_metadata_buffer length:actual_encrypted_metadata_len];
    [peripheral writeValue:dataToSend forCharacteristic:characteristic type:CBCharacteristicWriteWithResponse];
    transferState->current_chunk_index++; // Increment for the next data chunk

    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("File Transfer", [NSString stringWithFormat:@"Sent file metadata for '%s'. Size: %ld. Starting data transfer...", file_path, file_size].UTF8String);

    return true;
}

static bool macos_receiveFile(const char* device_address, const char* destination_path) {
    return true;
}

static IBluetoothManager macos_manager = {
    .discoverDevices = macos_discoverDevices,
    .pairDevice = macos_pairDevice,
    .connect = macos_connect,
    .disconnect = macos_disconnect,
    .sendMessage = macos_sendMessage,
    .receiveMessage = macos_receiveMessage,
    .sendFile = macos_sendFile,
    .receiveFile = macos_receiveFile
};

IBluetoothManager* get_macos_bluetooth_manager(void) {
    return &macos_manager;
}
