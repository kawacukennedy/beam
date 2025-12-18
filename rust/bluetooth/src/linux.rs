use crate::manager::BluetoothManager;
use crate::device::Device;
use crate::connection::Connection;
use crate::transport::Transport;
use crate::Result;
use bluer::{Adapter, Device as BluezDevice};
use std::sync::Arc;
use tokio::sync::mpsc;
use uuid::Uuid;

pub struct LinuxBluetoothManager {
    adapter: Adapter,
    discovered_devices: Arc<Mutex<HashMap<String, Arc<LinuxDevice>>>>,
}

impl LinuxBluetoothManager {
    pub async fn new() -> Result<Self> {
        let session = bluer::Session::new().await?;
        let adapter = session.default_adapter().await?;
        adapter.set_powered(true).await?;
        Ok(Self {
            adapter,
            discovered_devices: Arc::new(Mutex::new(HashMap::new())),
        })
    }
}

#[async_trait::async_trait]
impl BluetoothManager for LinuxBluetoothManager {
    async fn start_discovery(&self) -> Result<()> {
        self.adapter.start_discovery().await?;
        Ok(())
    }

    async fn stop_discovery(&self) -> Result<()> {
        self.adapter.stop_discovery().await?;
        Ok(())
    }

    fn discovered_devices(&self) -> Vec<Arc<dyn Device>> {
        self.discovered_devices.lock().unwrap().values().cloned().collect()
    }

    async fn connect(&self, device: &dyn Device) -> Result<Arc<dyn Connection>> {
        todo!()
    }
}

pub struct LinuxDevice {
    device: BluezDevice,
}

#[async_trait::async_trait]
impl Device for LinuxDevice {
    fn id(&self) -> String {
        self.device.address().to_string()
    }

    fn name(&self) -> Option<String> {
        self.device.name().await.ok().flatten()
    }

    fn address(&self) -> String {
        self.device.address().to_string()
    }

    fn is_connected(&self) -> bool {
        self.device.is_connected().await.unwrap_or(false)
    }

    fn services(&self) -> Vec<Uuid> {
        vec![]
    }
}

pub struct LinuxConnection;

#[async_trait::async_trait]
impl Connection for LinuxConnection {
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

pub struct LinuxTransport;

#[async_trait::async_trait]
impl Transport for LinuxTransport {
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