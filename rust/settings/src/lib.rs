use bluebeam_crypto::{encrypt_data, decrypt_data, CryptoError};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;
use dirs::config_dir;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum SettingsError {
    #[error("Failed to load settings: {source}")]
    LoadFailed { source: Box<dyn std::error::Error + Send + Sync> },
    #[error("Failed to save settings: {source}")]
    SaveFailed { source: Box<dyn std::error::Error + Send + Sync> },
    #[error("Invalid settings format: {message}")]
    InvalidFormat { message: String },
}

pub type Result<T> = std::result::Result<T, SettingsError>;

#[no_mangle]
pub extern "C" fn settings_get_user_name() -> *mut std::ffi::c_char {
    let manager = SettingsManager::new().unwrap_or_else(|_| {
        // Fallback to default settings if loading fails
        SettingsManager {
            settings: AppSettings::default(),
            file_path: PathBuf::from("/tmp/settings.enc"),
        }
    });
    let settings = manager.get_settings();
    std::ffi::CString::new(settings.user_name.clone()).unwrap().into_raw()
}

#[no_mangle]
pub extern "C" fn settings_set_user_name(name: *const std::ffi::c_char) {
    let c_str = unsafe { std::ffi::CStr::from_ptr(name) };
    let name_str = c_str.to_str().unwrap().to_string();
    let mut manager = SettingsManager::new().unwrap_or_else(|_| {
        // Fallback to default settings if loading fails
        SettingsManager {
            settings: AppSettings::default(),
            file_path: PathBuf::from("/tmp/settings.enc"),
        }
    });
    let _ = manager.update_settings(|s| s.user_name = name_str); // Ignore errors in FFI
}

// Add more FFI functions as needed

#[derive(Serialize, Deserialize, Debug)]
pub struct AppSettings {
    pub user_name: String,
    pub theme: String,
    pub download_path: String,
    pub encryption_enabled: bool,
    pub auto_lock_timeout: i32,
    pub biometric_auth_enabled: bool,
    pub two_factor_enabled: bool,
    pub language: String,
    pub notifications_enabled: bool,
    pub auto_update_enabled: bool,
    pub profile_picture_path: String,
    pub email: String,
    pub first_run: bool,
    pub trusted_devices: Vec<String>,
}

impl Default for AppSettings {
    fn default() -> Self {
        AppSettings {
            user_name: String::new(),
            theme: "light".to_string(),
            download_path: dirs::download_dir().unwrap_or(PathBuf::from("/tmp")).to_string_lossy().to_string(),
            encryption_enabled: true,
            auto_lock_timeout: 5,
            biometric_auth_enabled: false,
            two_factor_enabled: false,
            language: "en".to_string(),
            notifications_enabled: true,
            auto_update_enabled: true,
            profile_picture_path: String::new(),
            email: String::new(),
            first_run: true,
            trusted_devices: vec![],
        }
    }
}

pub struct SettingsManager {
    settings: AppSettings,
    file_path: PathBuf,
}

impl SettingsManager {
    pub fn new() -> Result<Self> {
        let mut config_dir = config_dir().ok_or(SettingsError::LoadFailed {
            source: Box::new(std::io::Error::new(std::io::ErrorKind::NotFound, "Config directory not found"))
        })?;
        config_dir.push("bluebeam");
        fs::create_dir_all(&config_dir).map_err(|e| SettingsError::LoadFailed { source: Box::new(e) })?;
        config_dir.push("settings.enc");

        let settings = if config_dir.exists() {
            Self::load_from_file(&config_dir)?
        } else {
            AppSettings::default()
        };

        Ok(SettingsManager {
            settings,
            file_path: config_dir,
        })
    }

    fn load_from_file(path: &PathBuf) -> Result<AppSettings> {
        let encrypted_data = fs::read(path).map_err(|e| SettingsError::LoadFailed { source: Box::new(e) })?;
        if encrypted_data.is_empty() {
            return Ok(AppSettings::default());
        }
        let key = b"01234567890123456789012345678901"; // Fixed key for demo
        match decrypt_data(key, &encrypted_data) {
            Ok(data) => serde_json::from_slice(&data).map_err(|e| SettingsError::InvalidFormat {
                message: format!("JSON parse error: {}", e)
            }),
            Err(e) => Err(SettingsError::LoadFailed { source: Box::new(e) }),
        }
    }

    pub fn save(&self) -> Result<()> {
        let data = serde_json::to_vec(&self.settings).map_err(|e| SettingsError::SaveFailed {
            source: Box::new(e)
        })?;
        let key = b"01234567890123456789012345678901";
        let encrypted = encrypt_data(key, &data).map_err(|e| SettingsError::SaveFailed {
            source: Box::new(e)
        })?;
        fs::write(&self.file_path, encrypted).map_err(|e| SettingsError::SaveFailed {
            source: Box::new(e)
        })?;
        Ok(())
    }

    pub fn get_settings(&self) -> &AppSettings {
        &self.settings
    }

    pub fn update_settings<F>(&mut self, updater: F) -> Result<()>
    where
        F: FnOnce(&mut AppSettings),
    {
        updater(&mut self.settings);
        self.save()
    }
}