#include "bluetooth.h"
#include <unordered_map>
#include <CoreBluetooth/CoreBluetooth.h>

@interface BluetoothDelegate : NSObject<CBCentralManagerDelegate, CBPeripheralDelegate>
@property (nonatomic, assign) Bluetooth* bluetooth;
@end

@implementation BluetoothDelegate

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    // Handle state updates
}

- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *,id> *)advertisementData RSSI:(NSNumber *)RSSI {
    if (self.bluetooth) {
        std::string device_id = [[peripheral.identifier UUIDString] UTF8String];
        self.bluetooth->add_discovered_device(device_id, peripheral);
    }
}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral {
    peripheral.delegate = self;
    [peripheral discoverServices:nil];
}

- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    std::string device_id = [[peripheral.identifier UUIDString] UTF8String];
    // Handle disconnection
    if (self.bluetooth) {
        self.bluetooth->call_disconnect_callback(device_id);
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error {
    if (!error) {
        for (CBService *service in peripheral.services) {
            [peripheral discoverCharacteristics:nil forService:service];
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error {
    if (!error) {
        std::string device_id = [[peripheral.identifier UUIDString] UTF8String];
        for (CBCharacteristic *characteristic in service.characteristics) {
            if (characteristic.properties & CBCharacteristicPropertyNotify) {
                [peripheral setNotifyValue:YES forCharacteristic:characteristic];
            }
            if (characteristic.properties & CBCharacteristicPropertyWrite) {
                self.bluetooth->set_tx_characteristic(device_id, characteristic);
            }
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    if (self.bluetooth && !error) {
        NSData *data = characteristic.value;
        if (data) {
            std::string device_id = [[peripheral.identifier UUIDString] UTF8String];
            std::vector<uint8_t> received_data((uint8_t*)data.bytes, (uint8_t*)data.bytes + data.length);
            self.bluetooth->receive_data(device_id, received_data);
        }
    }
}
@end

class Bluetooth::Impl {
public:
    CBCentralManager* centralManager;
    BluetoothDelegate* delegate;
    std::function<void(const std::string& device_id, const std::vector<uint8_t>& data)> receive_callback;
    std::function<void(const std::string& device_id)> disconnect_callback;
    std::unordered_map<std::string, CBPeripheral*> connections;
    std::unordered_map<std::string, CBPeripheral*> discovered_devices;
    std::unordered_map<std::string, std::string> name_to_id;
    std::unordered_map<std::string, CBCharacteristic*> tx_characteristics;

    Impl() {
        delegate = [[BluetoothDelegate alloc] init];
        delegate.bluetooth = nullptr;
        centralManager = [[CBCentralManager alloc] initWithDelegate:delegate queue:nil];
    }

    ~Impl() {
        [centralManager release];
        [delegate release];
    }
};

Bluetooth::Bluetooth() : pimpl(std::make_unique<Impl>()) {
    pimpl->delegate.bluetooth = this;
}

Bluetooth::~Bluetooth() = default;

void Bluetooth::scan() {
    pimpl->discovered_devices.clear();
    pimpl->name_to_id.clear();
    NSDictionary* options = @{CBCentralManagerScanOptionAllowDuplicatesKey: @NO};
    [pimpl->centralManager scanForPeripheralsWithServices:nil options:options];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        [pimpl->centralManager stopScan];
    });
}

bool Bluetooth::connect(const std::string& device_id) {
    auto it = pimpl->discovered_devices.find(device_id);
    if (it != pimpl->discovered_devices.end()) {
        CBPeripheral* peripheral = it->second;
        [pimpl->centralManager connectPeripheral:peripheral options:nil];
        pimpl->connections[device_id] = peripheral;
        return true;
    }
    return false;
}

bool Bluetooth::send_data(const std::string& device_id, const std::vector<uint8_t>& data) {
    auto it = pimpl->tx_characteristics.find(device_id);
    if (it != pimpl->tx_characteristics.end()) {
        CBCharacteristic* characteristic = it->second;
        auto conn_it = pimpl->connections.find(device_id);
        if (conn_it != pimpl->connections.end()) {
            CBPeripheral* peripheral = conn_it->second;
            NSData* nsdata = [NSData dataWithBytes:data.data() length:data.size()];
            [peripheral writeValue:nsdata forCharacteristic:characteristic type:CBCharacteristicWriteWithResponse];
            return true;
        }
    }
    return false;
}

void Bluetooth::set_receive_callback(std::function<void(const std::string& device_id, const std::vector<uint8_t>& data)> callback) {
    pimpl->receive_callback = callback;
}

void Bluetooth::set_disconnect_callback(std::function<void(const std::string& device_id)> callback) {
    pimpl->disconnect_callback = callback;
}

void Bluetooth::call_disconnect_callback(const std::string& device_id) {
    if (pimpl->disconnect_callback) {
        pimpl->disconnect_callback(device_id);
    }
}

std::vector<std::string> Bluetooth::get_discovered_devices() {
    std::vector<std::string> devices;
    for (const auto& pair : pimpl->name_to_id) {
        devices.push_back(pair.first);
    }
    return devices;
}

std::string Bluetooth::get_device_id_from_name(const std::string& name) {
    auto it = pimpl->name_to_id.find(name);
    if (it != pimpl->name_to_id.end()) {
        return it->second;
    }
    return name; // fallback
}

void Bluetooth::receive_data(const std::string& device_id, const std::vector<uint8_t>& data) {
    if (pimpl->receive_callback) {
        pimpl->receive_callback(device_id, data);
    }
}

void Bluetooth::add_discovered_device(const std::string& device_id, void* peripheral) {
    CBPeripheral* p = (CBPeripheral*)peripheral;
    pimpl->discovered_devices[device_id] = p;
    NSString* nsName = p.name;
    std::string name = nsName ? [nsName UTF8String] : device_id;
    pimpl->name_to_id[name] = device_id;
}

void Bluetooth::set_tx_characteristic(const std::string& device_id, void* characteristic) {
    pimpl->tx_characteristics[device_id] = (CBCharacteristic*)characteristic;
}