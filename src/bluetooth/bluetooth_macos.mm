#include "bluetooth_macos.h"
#include <stdio.h>
#include <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>
#include <string.h>
#include <errno.h>
#include "crypto_manager.h"
#include <sodium.h>
#include "file_transfer_worker.h" // Include the new worker header
#include <QThread>
#include <QMap>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QFileInfo>

#include "bluetooth_callbacks.h"

@interface BluetoothCentralManagerDelegate : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
@end

@interface BluetoothMainThreadBridge : NSObject
- (instancetype)initWithPeripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *)characteristic;
- (void)sendDataToCharacteristic:(NSData *)data;
@property (nonatomic, strong) CBPeripheral *peripheral;
@property (nonatomic, strong) CBCharacteristic *characteristic;
@end

@implementation BluetoothMainThreadBridge
- (instancetype)initWithPeripheral:(CBPeripheral *)peripheral characteristic:(CBCharacteristic *)characteristic {
    self = [super init];
    if (self) {
        _peripheral = peripheral;
        _characteristic = characteristic;
    }
    return self;
}

- (void)sendDataToCharacteristic:(NSData *)data {
    if (self.peripheral && self.characteristic) {
        [self.peripheral writeValue:data forCharacteristic:self.characteristic type:CBCharacteristicWriteWithResponse];
    }
}
@end

// Define custom service and characteristic UUIDs
// These are placeholder UUIDs. In a real application, you would generate your own unique UUIDs.
#define BLUEBEAM_SERVICE_UUID_STRING        @"E20A39F4-73F5-4BC4-A12F-17D1AD07A961"
#define MESSAGE_CHARACTERISTIC_UUID_STRING  @"08590F7E-DB05-467E-8757-72F6F6669999"
#define FILE_TRANSFER_CHARACTERISTIC_UUID_STRING @"08590F7E-DB05-467E-8757-72F6F6668888"

// Max data size for a single BLE write (considering MTU and overhead)
// This is a common value, but actual MTU can be negotiated.
#define MAX_BLE_WRITE_DATA_SIZE 500

// Global dictionaries to store FileTransferWorker instances and their threads, keyed by peripheral UUID string
static QMap<QString, QThread*> ongoingFileTransferThreads;
static QMap<QString, FileTransferWorker*> ongoingFileTransferWorkers;

// Define a global pointer to the CBCentralManager
static CBCentralManager *centralManager = nil;
// Array to store discovered peripherals
static NSMutableArray<CBPeripheral *> *discoveredPeripherals = nil;
// Dictionary to store connected peripherals by address (or UUID string)
static NSMutableDictionary<NSString *, CBPeripheral *> *connectedPeripherals = nil;

// Dictionary to store characteristics for connected peripherals
static NSMutableDictionary<NSString *, CBCharacteristic *> *messageCharacteristics = nil;
static NSMutableDictionary<NSString *, CBCharacteristic *> *fileTransferCharacteristics = nil;

// Global dictionaries to store connection timers for peripherals
static NSMutableDictionary<NSString *, NSTimer *> *connectionTimers = nil;

// Store a map of peripheral UUIDs to their main thread bridges
static NSMutableDictionary<NSString *, BluetoothMainThreadBridge *> *mainThreadBridges = nil;

@implementation BluetoothCentralManagerDelegate

- (instancetype)init
{
    self = [super init];
    if (self) {
        mainThreadBridges = [[NSMutableDictionary alloc] init];
    }
    return self;
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    // Ensure QCoreApplication is initialized for Qt's event loop
    // This block is removed as QApplication is initialized in main.cpp
    // if (!QCoreApplication::instance()) {
    //     static int argc = 0;
    //     QCoreApplication *app = new QCoreApplication(argc, nullptr);
    //     Q_UNUSED(app);
    // }

    switch (central.state) {
        case CBManagerStatePoweredOn: {
            if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", "CoreBluetooth State: Powered On. Starting scan...");
            [discoveredPeripherals removeAllObjects];
            if (g_bluetooth_ui_callbacks.clear_discovered_devices) g_bluetooth_ui_callbacks.clear_discovered_devices();
            CBUUID *bluebeamServiceUUID = [CBUUID UUIDWithString:BLUEBEAM_SERVICE_UUID_STRING];
            [central scanForPeripheralsWithServices:@[bluebeamServiceUUID] options:@{CBCentralManagerScanOptionAllowDuplicatesKey : @YES}];
            break;
        }
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

    // Create and store a BluetoothMainThreadBridge for this peripheral
    BluetoothMainThreadBridge *bridge = [[BluetoothMainThreadBridge alloc] initWithPeripheral:peripheral characteristic:nil];
    [mainThreadBridges setObject:bridge forKey:peripheral.identifier.UUIDString];

    // Invalidate and remove connection timer
    NSTimer *timer = [connectionTimers objectForKey:peripheral.identifier.UUIDString];
    if (timer) {
        [timer invalidate];
        [connectionTimers removeObjectForKey:peripheral.identifier.UUIDString];
    }
}

- (void)centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Failed to connect to peripheral %@: %@", peripheral.name, error.localizedDescription].UTF8String);
    if (g_bluetooth_ui_callbacks.update_device_connection_status) g_bluetooth_ui_callbacks.update_device_connection_status(peripheral.identifier.UUIDString.UTF8String, false);

    // Invalidate and remove connection timer
    NSTimer *timer = [connectionTimers objectForKey:peripheral.identifier.UUIDString];
    if (timer) {
        [timer invalidate];
        [connectionTimers removeObjectForKey:peripheral.identifier.UUIDString];
    }
}

- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Disconnected from peripheral %@: %@", peripheral.name, error ? error.localizedDescription : @"(null)"].UTF8String);
    if (g_bluetooth_ui_callbacks.update_device_connection_status) g_bluetooth_ui_callbacks.update_device_connection_status(peripheral.identifier.UUIDString.UTF8String, false);
    [connectedPeripherals removeObjectForKey:peripheral.identifier.UUIDString];
    [messageCharacteristics removeObjectForKey:peripheral.identifier.UUIDString];
    [fileTransferCharacteristics removeObjectForKey:peripheral.identifier.UUIDString];

    // Clean up FileTransferWorker and its QThread if a transfer was ongoing
    QString peripheralId = QString::fromNSString(peripheral.identifier.UUIDString);
    if (ongoingFileTransferWorkers.contains(peripheralId)) {
        FileTransferWorker *worker = ongoingFileTransferWorkers.value(peripheralId);
        QThread *thread = ongoingFileTransferThreads.value(peripheralId);

        if (worker) {
            worker->deleteLater();
        }
        if (thread) {
            thread->quit();
            thread->wait(1000); // Wait for the thread to finish, with a timeout
            thread->deleteLater();
        }
        ongoingFileTransferWorkers.remove(peripheralId);
        ongoingFileTransferThreads.remove(peripheralId);
    }

    // Remove the BluetoothMainThreadBridge instance
    [mainThreadBridges removeObjectForKey:peripheral.identifier.UUIDString];

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

            // Update the bridge with the file transfer characteristic
            BluetoothMainThreadBridge *bridge = [mainThreadBridges objectForKey:peripheral.identifier.UUIDString];
            if (bridge) {
                bridge.characteristic = characteristic;
            }
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
            // File transfer logic removed for simplification
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didWriteValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    if (error) {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Error writing value for characteristic %@: %@", characteristic.UUID.UUIDString, error.localizedDescription].UTF8String);
    } else {
        // Write confirmation logic removed for simplification
    }
}

@end



// C functions for Bluetooth management
static void macos_discoverDevices(void) {
    if (!centralManager) {
        centralManager = [[CBCentralManager alloc] initWithDelegate:[[BluetoothCentralManagerDelegate alloc] init] queue:dispatch_get_main_queue()];
        discoveredPeripherals = [[NSMutableArray alloc] init];
        connectedPeripherals = [[NSMutableDictionary alloc] init];
        messageCharacteristics = [[NSMutableDictionary alloc] init];
        fileTransferCharacteristics = [[NSMutableDictionary alloc] init];
        mainThreadBridges = [[NSMutableDictionary alloc] init];
    } else {
        if (centralManager.state == CBManagerStatePoweredOn) {
            [discoveredPeripherals removeAllObjects];
            if (g_bluetooth_ui_callbacks.clear_discovered_devices) g_bluetooth_ui_callbacks.clear_discovered_devices();
            CBUUID *bluebeamServiceUUID = [CBUUID UUIDWithString:BLUEBEAM_SERVICE_UUID_STRING];
            [centralManager scanForPeripheralsWithServices:@[bluebeamServiceUUID] options:@{CBCentralManagerScanOptionAllowDuplicatesKey : @YES}];
        }
    }
}

static void connectionTimeout(NSTimer* timer) {
    NSString *deviceAddress = timer.userInfo;
    CBPeripheral *targetPeripheral = [connectedPeripherals objectForKey:deviceAddress];
    if (targetPeripheral) {
        [centralManager cancelPeripheralConnection:targetPeripheral];
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Connection to %@ timed out.", deviceAddress].UTF8String);
    }
    [connectionTimers removeObjectForKey:deviceAddress];
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

        // Start connection timeout timer
        NSTimer *timer = [NSTimer scheduledTimerWithTimeInterval:10.0 // 10 seconds timeout
                                                          target:[BluetoothCentralManagerDelegate new] // Use a new delegate instance for the timer target
                                                        selector:@selector(connectionTimeout:)
                                                        userInfo:[NSString stringWithUTF8String:device_address]
                                                         repeats:NO];
        [connectionTimers setObject:timer forKey:[NSString stringWithUTF8String:device_address]];

        return true;
    } else {
        if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"Error: Peripheral with address %s not found.", device_address].UTF8String);
        return false;
    }
}

static bool macos_pairDevice(const char* device_address) {
    // CoreBluetooth handles pairing implicitly during connection if required.
    if (g_bluetooth_ui_callbacks.show_alert) g_bluetooth_ui_callbacks.show_alert("Bluetooth", [NSString stringWithFormat:@"macOS: Attempting to connect to device %s. Pairing will be handled implicitly by the system if required.", device_address].UTF8String);
    return macos_connect(device_address);
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
    // File transfer logic removed for simplification
    return false;
}

static bool macos_receiveFile(const char* device_address, const char* destination_path) {
    // File transfer logic removed for simplification
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

static void initialize_macos_bluetooth_globals() {
    if (!discoveredPeripherals) {
        discoveredPeripherals = [[NSMutableArray alloc] init];
        connectedPeripherals = [[NSMutableDictionary alloc] init];
        messageCharacteristics = [[NSMutableDictionary alloc] init];
        fileTransferCharacteristics = [[NSMutableDictionary alloc] init];
        connectionTimers = [[NSMutableDictionary alloc] init];
        mainThreadBridges = [[NSMutableDictionary alloc] init];
    }
}

IBluetoothManager* get_macos_bluetooth_manager(void) {
    initialize_macos_bluetooth_globals();
    return &macos_manager;
}
