#[cfg(target_os = "linux")]
use bluer::{Adapter, Device, DiscoveryFilter, DiscoveryTransport};
#[cfg(target_os = "linux")]
use futures::stream::StreamExt;

#[cfg(target_os = "macos")]


#[cfg(target_os = "windows")]
use winrt::windows::devices::bluetooth as bt;

pub struct BluetoothManager {
    devices: Vec<DeviceInfo>,
    #[cfg(target_os = "linux")]
    session: Option<bluer::Session>,
    #[cfg(target_os = "linux")]
    adapter: Option<bluer::Adapter>,
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
            #[cfg(target_os = "linux")]
            session: None,
            #[cfg(target_os = "linux")]
            adapter: None,
        }
    }

    #[cfg(target_os = "linux")]
    pub fn scan_devices(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let rt = tokio::runtime::Runtime::new()?;
        rt.block_on(async {
            let session = bluer::Session::new().await?;
            let adapter = session.default_adapter().await?;
            adapter.set_powered(true).await?;
            let mut filter = DiscoveryFilter::default();
            filter.transport = DiscoveryTransport::Le;
            adapter.set_discovery_filter(filter).await?;
            let discover = adapter.discover_devices().await?;
            tokio::pin!(discover);
            while let Some(evt) = discover.next().await {
                match evt {
                    AdapterEvent::DeviceAdded(addr) => {
                        if let Ok(device) = adapter.device(addr) {
                            if let Ok(name) = device.name().await {
                                let name = name.unwrap_or_else(|| addr.to_string());
                                let rssi = device.rssi().await.unwrap_or(0);
                                self.devices.push(DeviceInfo {
                                    id: addr.to_string(),
                                    name,
                                    address: addr.to_string(),
                                    rssi,
                                });
                            }
                        }
                    }
                    _ => {}
                }
            }
            Ok(())
        })
    }

    #[cfg(target_os = "macos")]
    pub fn scan_devices(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        // macOS CoreBluetooth scanning
        // This would require async runtime and CoreBluetooth bindings
        Ok(())
    }

    #[cfg(target_os = "windows")]
    pub fn scan_devices(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        // Windows Bluetooth scanning - placeholder for winrt implementation
        // Requires winrt bindings for Windows.Devices.Bluetooth
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
        unsafe { let _ = Box::from_raw(ptr); }
    }
}