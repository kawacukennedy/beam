pub mod manager;
pub mod device;
pub mod connection;
pub mod transport;

#[cfg(target_os = "macos")]
mod macos;

#[cfg(target_os = "windows")]
mod windows;

#[cfg(target_os = "linux")]
mod linux;

use std::error::Error;
use std::fmt;

#[derive(Debug)]
pub struct BluetoothError(String);

impl fmt::Display for BluetoothError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Bluetooth error: {}", self.0)
    }
}

impl Error for BluetoothError {}

pub type Result<T> = std::result::Result<T, BluetoothError>;