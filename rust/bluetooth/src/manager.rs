use crate::device::Device;
use crate::Result;
use std::sync::Arc;
use tokio::sync::mpsc;

#[async_trait::async_trait]
pub trait BluetoothManager: Send + Sync {
    async fn start_discovery(&self) -> Result<()>;
    async fn stop_discovery(&self) -> Result<()>;
    fn discovered_devices(&self) -> Vec<Arc<dyn Device>>;
    async fn connect(&self, device: &dyn Device) -> Result<Arc<dyn Connection>>;
}

pub struct DiscoveryEvent {
    pub device: Arc<dyn Device>,
}

#[cfg(target_os = "macos")]
use crate::macos::MacOSBluetoothManager;

#[cfg(target_os = "windows")]
use crate::windows::WindowsBluetoothManager;

#[cfg(target_os = "linux")]
use crate::linux::LinuxBluetoothManager;

pub async fn create_manager() -> Result<Arc<dyn BluetoothManager>> {
    #[cfg(target_os = "macos")]
    {
        Ok(Arc::new(MacOSBluetoothManager::new().await?))
    }
    #[cfg(target_os = "windows")]
    {
        Ok(Arc::new(WindowsBluetoothManager::new().await?))
    }
    #[cfg(target_os = "linux")]
    {
        Ok(Arc::new(LinuxBluetoothManager::new().await?))
    }
}