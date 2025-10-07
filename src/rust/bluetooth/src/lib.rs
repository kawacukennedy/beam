#[cfg(target_os = "linux")]
use bluer::{Adapter, Device};

#[cfg(target_os = "macos")]
use corebluetooth_rs as cb;

#[cfg(target_os = "windows")]
use winrt::windows::devices::bluetooth as bt;

pub struct BluetoothManager {
    devices: Vec<DeviceInfo>,
}

#[derive(Clone)]
pub struct DeviceInfo {
    pub id: String,
    pub name: String,
    pub address: String,
    pub rssi: i16,
}

impl BluetoothManager {
    pub fn new() -> Self {
        BluetoothManager {
            devices: Vec::new(),
        }
    }

    #[cfg(target_os = "linux")]
    pub async fn scan_devices(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let session = bluer::Session::new().await?;
        let adapter = session.default_adapter().await?;
        adapter.set_powered(true).await?;

        let device_events = adapter.discover_devices().await?;
        tokio::pin!(device_events);

        while let Some(evt) = device_events.next().await {
            if let bluer::AdapterEvent::DeviceAdded(addr) = evt {
                let device = adapter.device(addr)?;
                let name = device.name().await?.unwrap_or_else(|| "Unknown".to_string());
                let rssi = device.rssi().await?.unwrap_or(0);

                self.devices.push(DeviceInfo {
                    id: addr.to_string(),
                    name,
                    address: addr.to_string(),
                    rssi,
                });
            }
        }

        Ok(())
    }

    #[cfg(target_os = "macos")]
    pub fn scan_devices(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        // Placeholder for macOS CoreBluetooth
        Ok(())
    }

    #[cfg(target_os = "windows")]
    pub fn scan_devices(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        // Placeholder for Windows Bluetooth
        Ok(())
    }

    pub fn get_devices(&self) -> Vec<DeviceInfo> {
        self.devices.clone()
    }
}

#[no_mangle]
pub extern "C" fn bluetooth_new() -> *mut BluetoothManager {
    Box::into_raw(Box::new(BluetoothManager::new()))
}

#[no_mangle]
pub extern "C" fn bluetooth_free(ptr: *mut BluetoothManager) {
    if !ptr.is_null() {
        unsafe { Box::from_raw(ptr); }
    }
}