pub mod manager;
pub mod device;
pub mod connection;
pub mod transport;
pub mod pairing;

#[cfg(target_os = "macos")]
mod macos;

#[cfg(target_os = "windows")]
mod windows;

#[cfg(target_os = "linux")]
mod linux;

use std::error::Error;
use std::fmt;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum BluetoothError {
    #[error("Bluetooth not supported on this platform")]
    NotSupported,
    #[error("Bluetooth is disabled")]
    Disabled,
    #[error("Device discovery failed: {source}")]
    DiscoveryFailed { source: Box<dyn std::error::Error + Send + Sync> },
    #[error("Connection failed: {source}")]
    ConnectionFailed { source: Box<dyn std::error::Error + Send + Sync> },
    #[error("Pairing failed: {source}")]
    PairingFailed { source: Box<dyn std::error::Error + Send + Sync> },
    #[error("Device not found: {device_id}")]
    DeviceNotFound { device_id: String },
}

pub type Result<T> = std::result::Result<T, BluetoothError>;