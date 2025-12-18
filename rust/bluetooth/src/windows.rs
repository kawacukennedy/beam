use crate::manager::BluetoothManager;
use crate::device::Device;
use crate::connection::Connection;
use crate::transport::Transport;
use crate::Result;
use std::sync::Arc;
use tokio::sync::mpsc;
use uuid::Uuid;
use windows::Devices::Bluetooth::{BluetoothAdapter, BluetoothDevice};
use windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher;

pub struct WindowsBluetoothManager {
    adapter: BluetoothAdapter,
    watcher: BluetoothLEAdvertisementWatcher,
    discovered_devices: Arc<Mutex<HashMap<String, Arc<WindowsDevice>>>>,
}

impl WindowsBluetoothManager {
    pub async fn new() -> Result<Self> {
        let adapter = BluetoothAdapter::GetDefaultAsync()?.await?;
        let watcher = BluetoothLEAdvertisementWatcher::new()?;
        Ok(Self {
            adapter,
            watcher,
            discovered_devices: Arc::new(Mutex::new(HashMap::new())),
        })
    }
}

#[async_trait::async_trait]
impl BluetoothManager for WindowsBluetoothManager {
    async fn start_discovery(&self) -> Result<()> {
        self.watcher.Start()?;
        Ok(())
    }

    async fn stop_discovery(&self) -> Result<()> {
        self.watcher.Stop()?;
        Ok(())
    }

    fn discovered_devices(&self) -> Vec<Arc<dyn Device>> {
        self.discovered_devices.lock().unwrap().values().cloned().collect()
    }

    async fn connect(&self, device: &dyn Device) -> Result<Arc<dyn Connection>> {
        todo!()
    }
}

pub struct WindowsDevice {
    device: BluetoothDevice,
}

#[async_trait::async_trait]
impl Device for WindowsDevice {
    fn id(&self) -> String {
        self.device.BluetoothAddress().unwrap().to_string()
    }

    fn name(&self) -> Option<String> {
        self.device.Name().ok()
    }

    fn address(&self) -> String {
        self.id()
    }

    fn is_connected(&self) -> bool {
        self.device.ConnectionStatus().unwrap() == windows::Devices::Bluetooth::BluetoothConnectionStatus::Connected
    }

    fn services(&self) -> Vec<Uuid> {
        vec![]
    }
}

pub struct WindowsConnection;

#[async_trait::async_trait]
impl Connection for WindowsConnection {
    async fn disconnect(&self) -> Result<()> {
        todo!()
    }

    fn is_connected(&self) -> bool {
        todo!()
    }

    async fn create_transport(&self, service_uuid: Uuid) -> Result<Arc<dyn Transport>> {
        todo!()
    }
}

pub struct WindowsTransport;

#[async_trait::async_trait]
impl Transport for WindowsTransport {
    async fn send_packet(&self, data: &[u8]) -> Result<()> {
        todo!()
    }

    async fn receive_packets(&self) -> Result<mpsc::Receiver<Vec<u8>>> {
        todo!()
    }

    async fn close(&self) -> Result<()> {
        todo!()
    }
}