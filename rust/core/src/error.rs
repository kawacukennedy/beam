//! Error handling system for Bluebeam
//!
//! This module provides a comprehensive error handling system with:
//! - Low-level error codes for internal use
//! - Domain-specific error enums for different subsystems
//! - User-safe error messages that don't expose internal details
//! - Typed error propagation using Result types
//!
//! # Example
//! ```
//! use bluebeam_core::{Core, Error};
//!
//! let result = Core::new("key", None);
//! match result {
//!     Ok(core) => println!("Core initialized successfully"),
//!     Err(e) => {
//!         eprintln!("Error: {}", e.user_message());
//!         eprintln!("Code: {}", e.error_code() as u32);
//!     }
//! }
//! ```

use std::fmt;
use thiserror::Error;

// Re-export for convenience
pub use bluebeam_database::DatabaseError as ExternalDatabaseError;
pub use bluebeam_bluetooth::BluetoothError as ExternalBluetoothError;
pub use bluebeam_crypto::CryptoError as ExternalCryptoError;
pub use bluebeam_settings::SettingsError as ExternalSettingsError;

/// Low-level error codes for internal use
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ErrorCode {
    // General errors (1000-1999)
    Unknown = 1000,
    InvalidInput = 1001,
    PermissionDenied = 1002,
    NotFound = 1003,
    AlreadyExists = 1004,
    Timeout = 1005,

    // Database errors (2000-2999)
    DatabaseConnectionFailed = 2000,
    DatabaseQueryFailed = 2001,
    DatabaseMigrationFailed = 2002,
    DatabaseIntegrityError = 2003,

    // Bluetooth errors (3000-3999)
    BluetoothNotSupported = 3000,
    BluetoothDisabled = 3001,
    BluetoothDiscoveryFailed = 3002,
    BluetoothConnectionFailed = 3003,
    BluetoothPairingFailed = 3004,
    BluetoothDeviceNotFound = 3005,

    // Crypto errors (4000-4999)
    CryptoKeyExchangeFailed = 4000,
    CryptoInvalidKey = 4001,
    CryptoEncryptionFailed = 4002,
    CryptoDecryptionFailed = 4003,

    // Settings errors (5000-5999)
    SettingsLoadFailed = 5000,
    SettingsSaveFailed = 5001,
    SettingsInvalidFormat = 5002,

    // File transfer errors (6000-6999)
    FileTransferFailed = 6000,
    FileNotFound = 6001,
    FilePermissionDenied = 6002,
    FileTooLarge = 6003,
}

/// Domain-specific error enums
#[derive(Debug, Error)]
pub enum DatabaseError {
    #[error("Database connection failed: {source}")]
    ConnectionFailed { source: Box<dyn std::error::Error + Send + Sync> },
    #[error("Database query failed: {source}")]
    QueryFailed { source: Box<dyn std::error::Error + Send + Sync> },
    #[error("Database migration failed: {source}")]
    MigrationFailed { source: Box<dyn std::error::Error + Send + Sync> },
    #[error("Database integrity error: {message}")]
    IntegrityError { message: String },
    #[error("IO error: {source}")]
    Io { #[from] source: std::io::Error },
}

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

#[derive(Debug, Error)]
pub enum CryptoError {
    #[error("Key exchange failed")]
    KeyExchangeFailed,
    #[error("Invalid key provided")]
    InvalidKey,
    #[error("Encryption failed: {source}")]
    EncryptionFailed { source: Box<dyn std::error::Error + Send + Sync> },
    #[error("Decryption failed: {source}")]
    DecryptionFailed { source: Box<dyn std::error::Error + Send + Sync> },
}

#[derive(Debug, Error)]
pub enum SettingsError {
    #[error("Failed to load settings: {source}")]
    LoadFailed { source: Box<dyn std::error::Error + Send + Sync> },
    #[error("Failed to save settings: {source}")]
    SaveFailed { source: Box<dyn std::error::Error + Send + Sync> },
    #[error("Invalid settings format: {message}")]
    InvalidFormat { message: String },
}

#[derive(Debug, Error)]
pub enum FileTransferError {
    #[error("File transfer failed: {source}")]
    TransferFailed { source: Box<dyn std::error::Error + Send + Sync> },
    #[error("File not found: {path}")]
    FileNotFound { path: String },
    #[error("Permission denied for file: {path}")]
    PermissionDenied { path: String },
    #[error("File too large: {size} bytes")]
    FileTooLarge { size: u64 },
}

/// Top-level application error enum
#[derive(Debug, Error)]
pub enum Error {
    #[error("Database error: {0}")]
    Database(DatabaseError),
    #[error("Bluetooth error: {0}")]
    Bluetooth(BluetoothError),
    #[error("Crypto error: {0}")]
    Crypto(CryptoError),
    #[error("Settings error: {0}")]
    Settings(SettingsError),
    #[error("File transfer error: {0}")]
    FileTransfer(FileTransferError),
    #[error("General error: {message}")]
    General { message: String, code: ErrorCode },
}

impl From<bluebeam_database::DatabaseError> for Error {
    fn from(err: bluebeam_database::DatabaseError) -> Self {
        Error::Database(match err {
            bluebeam_database::DatabaseError::ConnectionFailed { source } => DatabaseError::ConnectionFailed { source },
            bluebeam_database::DatabaseError::QueryFailed { source } => DatabaseError::QueryFailed { source },
            bluebeam_database::DatabaseError::MigrationFailed { source } => DatabaseError::MigrationFailed { source },
            bluebeam_database::DatabaseError::IntegrityError { message } => DatabaseError::IntegrityError { message },
            bluebeam_database::DatabaseError::Io(source) => DatabaseError::Io { source },
        })
    }
}

impl From<bluebeam_bluetooth::BluetoothError> for Error {
    fn from(err: bluebeam_bluetooth::BluetoothError) -> Self {
        Error::Bluetooth(match err {
            bluebeam_bluetooth::BluetoothError::NotSupported => BluetoothError::NotSupported,
            bluebeam_bluetooth::BluetoothError::Disabled => BluetoothError::Disabled,
            bluebeam_bluetooth::BluetoothError::DiscoveryFailed { source } => BluetoothError::DiscoveryFailed { source },
            bluebeam_bluetooth::BluetoothError::ConnectionFailed { source } => BluetoothError::ConnectionFailed { source },
            bluebeam_bluetooth::BluetoothError::PairingFailed { source } => BluetoothError::PairingFailed { source },
            bluebeam_bluetooth::BluetoothError::DeviceNotFound { device_id } => BluetoothError::DeviceNotFound { device_id },
        })
    }
}

impl From<bluebeam_crypto::CryptoError> for Error {
    fn from(err: bluebeam_crypto::CryptoError) -> Self {
        Error::Crypto(match err {
            bluebeam_crypto::CryptoError::KeyExchangeFailed => CryptoError::KeyExchangeFailed,
            bluebeam_crypto::CryptoError::InvalidKey => CryptoError::InvalidKey,
            bluebeam_crypto::CryptoError::EncryptionFailed { source } => CryptoError::EncryptionFailed { source },
            bluebeam_crypto::CryptoError::DecryptionFailed { source } => CryptoError::DecryptionFailed { source },
        })
    }
}

impl From<bluebeam_settings::SettingsError> for Error {
    fn from(err: bluebeam_settings::SettingsError) -> Self {
        Error::Settings(match err {
            bluebeam_settings::SettingsError::LoadFailed { source } => SettingsError::LoadFailed { source },
            bluebeam_settings::SettingsError::SaveFailed { source } => SettingsError::SaveFailed { source },
            bluebeam_settings::SettingsError::InvalidFormat { message } => SettingsError::InvalidFormat { message },
        })
    }
}

/// User-facing error messages mapping
impl Error {
    pub fn user_message(&self) -> &'static str {
        match self {
            // Database errors
            Error::Database(DatabaseError::ConnectionFailed { .. }) =>
                "Unable to connect to the database. Please check your database configuration.",
            Error::Database(DatabaseError::QueryFailed { .. }) =>
                "A database operation failed. Please try again or contact support if the problem persists.",
            Error::Database(DatabaseError::MigrationFailed { .. }) =>
                "Database update failed. The application may need to be reinstalled.",
            Error::Database(DatabaseError::IntegrityError { .. }) =>
                "Database integrity check failed. Your data may be corrupted.",
            Error::Database(DatabaseError::Io { .. }) =>
                "File system error occurred while accessing the database.",

            // Bluetooth errors
            Error::Bluetooth(BluetoothError::NotSupported) =>
                "Bluetooth is not supported on this device.",
            Error::Bluetooth(BluetoothError::Disabled) =>
                "Bluetooth is disabled. Please enable Bluetooth in your system settings.",
            Error::Bluetooth(BluetoothError::DiscoveryFailed { .. }) =>
                "Failed to discover nearby devices. Please check your Bluetooth settings.",
            Error::Bluetooth(BluetoothError::ConnectionFailed { .. }) =>
                "Failed to connect to the device. Please ensure the device is nearby and try again.",
            Error::Bluetooth(BluetoothError::PairingFailed { .. }) =>
                "Device pairing failed. Please verify the PIN code and try again.",
            Error::Bluetooth(BluetoothError::DeviceNotFound { .. }) =>
                "The requested device could not be found.",

            // Crypto errors
            Error::Crypto(CryptoError::KeyExchangeFailed) =>
                "Secure key exchange failed. Please restart the pairing process.",
            Error::Crypto(CryptoError::InvalidKey) =>
                "Invalid encryption key detected. Your security may be compromised.",
            Error::Crypto(CryptoError::EncryptionFailed { .. }) =>
                "Failed to encrypt data. Please try again.",
            Error::Crypto(CryptoError::DecryptionFailed { .. }) =>
                "Failed to decrypt data. The data may be corrupted.",

            // Settings errors
            Error::Settings(SettingsError::LoadFailed { .. }) =>
                "Failed to load application settings. Default settings will be used.",
            Error::Settings(SettingsError::SaveFailed { .. }) =>
                "Failed to save your settings. Your changes may not be preserved.",
            Error::Settings(SettingsError::InvalidFormat { .. }) =>
                "Your settings file is corrupted. Default settings will be restored.",

            // File transfer errors
            Error::FileTransfer(FileTransferError::TransferFailed { .. }) =>
                "File transfer failed. Please check your connection and try again.",
            Error::FileTransfer(FileTransferError::FileNotFound { .. }) =>
                "The requested file could not be found.",
            Error::FileTransfer(FileTransferError::PermissionDenied { .. }) =>
                "You don't have permission to access this file.",
            Error::FileTransfer(FileTransferError::FileTooLarge { .. }) =>
                "The file is too large to transfer.",

            // General errors
            Error::General { code: ErrorCode::Unknown, .. } =>
                "An unexpected error occurred. Please try again.",
            Error::General { code: ErrorCode::InvalidInput, .. } =>
                "Invalid input provided. Please check your data and try again.",
            Error::General { code: ErrorCode::PermissionDenied, .. } =>
                "Permission denied. You may not have the required access rights.",
            Error::General { code: ErrorCode::NotFound, .. } =>
                "The requested item could not be found.",
            Error::General { code: ErrorCode::AlreadyExists, .. } =>
                "The item already exists.",
            Error::General { code: ErrorCode::Timeout, .. } =>
                "The operation timed out. Please try again.",
            _ => "An error occurred. Please try again or contact support.",
        }
    }

    pub fn error_code(&self) -> ErrorCode {
        match self {
            Error::Database(DatabaseError::ConnectionFailed { .. }) => ErrorCode::DatabaseConnectionFailed,
            Error::Database(DatabaseError::QueryFailed { .. }) => ErrorCode::DatabaseQueryFailed,
            Error::Database(DatabaseError::MigrationFailed { .. }) => ErrorCode::DatabaseMigrationFailed,
            Error::Database(DatabaseError::IntegrityError { .. }) => ErrorCode::DatabaseIntegrityError,
            Error::Database(DatabaseError::Io { .. }) => ErrorCode::DatabaseConnectionFailed,

            Error::Bluetooth(BluetoothError::NotSupported) => ErrorCode::BluetoothNotSupported,
            Error::Bluetooth(BluetoothError::Disabled) => ErrorCode::BluetoothDisabled,
            Error::Bluetooth(BluetoothError::DiscoveryFailed { .. }) => ErrorCode::BluetoothDiscoveryFailed,
            Error::Bluetooth(BluetoothError::ConnectionFailed { .. }) => ErrorCode::BluetoothConnectionFailed,
            Error::Bluetooth(BluetoothError::PairingFailed { .. }) => ErrorCode::BluetoothPairingFailed,
            Error::Bluetooth(BluetoothError::DeviceNotFound { .. }) => ErrorCode::BluetoothDeviceNotFound,

            Error::Crypto(CryptoError::KeyExchangeFailed) => ErrorCode::CryptoKeyExchangeFailed,
            Error::Crypto(CryptoError::InvalidKey) => ErrorCode::CryptoInvalidKey,
            Error::Crypto(CryptoError::EncryptionFailed { .. }) => ErrorCode::CryptoEncryptionFailed,
            Error::Crypto(CryptoError::DecryptionFailed { .. }) => ErrorCode::CryptoDecryptionFailed,

            Error::Settings(SettingsError::LoadFailed { .. }) => ErrorCode::SettingsLoadFailed,
            Error::Settings(SettingsError::SaveFailed { .. }) => ErrorCode::SettingsSaveFailed,
            Error::Settings(SettingsError::InvalidFormat { .. }) => ErrorCode::SettingsInvalidFormat,

            Error::FileTransfer(FileTransferError::TransferFailed { .. }) => ErrorCode::FileTransferFailed,
            Error::FileTransfer(FileTransferError::FileNotFound { .. }) => ErrorCode::FileNotFound,
            Error::FileTransfer(FileTransferError::PermissionDenied { .. }) => ErrorCode::FilePermissionDenied,
            Error::FileTransfer(FileTransferError::FileTooLarge { .. }) => ErrorCode::FileTooLarge,

            Error::General { code, .. } => *code,
        }
    }
}

pub type Result<T> = std::result::Result<T, Error>;