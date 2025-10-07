const noble = require('@abandonware/noble');
const cryptoManager = require('../crypto/crypto');

class BluetoothManager {
  constructor() {
    this.devices = new Map();
    this.isScanning = false;
    this.sharedSecrets = new Map(); // deviceId -> sharedSecret
  }

  async startScanning() {
    if (this.isScanning) return;

    return new Promise((resolve, reject) => {
      noble.on('stateChange', (state) => {
        if (state === 'poweredOn') {
          noble.startScanning([], true);
          this.isScanning = true;
          resolve();
        } else {
          reject(new Error('Bluetooth not powered on'));
        }
      });

      noble.on('discover', (peripheral) => {
        this.devices.set(peripheral.id, {
          id: peripheral.id,
          name: peripheral.advertisement.localName || 'Unknown Device',
          address: peripheral.address,
          rssi: peripheral.rssi,
          signalStrength: this.calculateSignalStrength(peripheral.rssi),
        });
      });

      // Stop scanning after 10 seconds
      setTimeout(() => {
        noble.stopScanning();
        this.isScanning = false;
      }, 10000);
    });
  }

  stopScanning() {
    noble.stopScanning();
    this.isScanning = false;
  }

  getDiscoveredDevices() {
    return Array.from(this.devices.values());
  }

  calculateSignalStrength(rssi) {
    // Simple calculation, RSSI typically -100 to 0
    if (rssi >= -50) return 100;
    if (rssi <= -100) return 0;
    return 2 * (rssi + 100);
  }

  async connectToDevice(deviceId) {
    const peripheral = noble._peripherals.get(deviceId);
    if (!peripheral) throw new Error('Device not found');

    return new Promise((resolve, reject) => {
      peripheral.connect((error) => {
        if (error) reject(error);
        else {
          // Discover services and characteristics
          peripheral.discoverServices([], (error, services) => {
            if (error) reject(error);
            else resolve({ peripheral, services });
          });
        }
      });
    });
  }

  async sendMessage(deviceId, message) {
    const sharedSecret = this.sharedSecrets.get(deviceId);
    if (!sharedSecret) throw new Error('No shared secret established');

    const encrypted = cryptoManager.encryptMessage(sharedSecret, message);
    // TODO: Send encrypted data over Bluetooth
    console.log('Sending encrypted message to', deviceId, ':', encrypted);
    return true;
  }
}

module.exports = new BluetoothManager();