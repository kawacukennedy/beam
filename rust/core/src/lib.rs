pub mod error;

use bluebeam_database::{Database, DatabaseResult, Device, Message, File, FileStatus};
use bluebeam_bluetooth::{manager::BluetoothManager, pairing::PairingSession};
use bluebeam_settings::SettingsManager;
use std::sync::Mutex;
use chrono::Utc;

pub use error::{Error, Result as CoreResult};

pub struct Core {
    db: Mutex<Database>,
    bluetooth_manager: Option<std::sync::Arc<dyn BluetoothManager>>,
    settings: Mutex<SettingsManager>,
}

impl Core {
    pub fn new(db_key: &str, bluetooth_manager: Option<std::sync::Arc<dyn BluetoothManager>>) -> CoreResult<Self> {
        let db = Database::new(db_key).map_err(Error::Database)?;
        let settings = SettingsManager::new().map_err(Error::Settings)?;
        Ok(Core {
            db: Mutex::new(db),
            bluetooth_manager,
            settings: Mutex::new(settings),
        })
    }

    pub fn add_device(&self, device: &Device) -> CoreResult<()> {
        self.db.lock().unwrap().add_device(device).map_err(Error::Database)
    }

    pub fn get_devices(&self) -> CoreResult<Vec<Device>> {
        self.db.lock().unwrap().get_devices().map_err(Error::Database)
    }

    pub fn add_message(&self, message: &Message) -> CoreResult<()> {
        self.db.lock().unwrap().add_message(message).map_err(Error::Database)
    }

    pub fn get_messages(&self, conversation_id: &str) -> CoreResult<Vec<Message>> {
        self.db.lock().unwrap().get_messages(conversation_id).map_err(Error::Database)
    }

    pub fn add_file(&self, file: &File) -> CoreResult<()> {
        self.db.lock().unwrap().add_file(file).map_err(Error::Database)
    }

    pub fn update_file_status(&self, id: &str, status: FileStatus) -> CoreResult<()> {
        self.db.lock().unwrap().update_file_status(id, status).map_err(Error::Database)
    }

    pub fn get_files(&self) -> CoreResult<Vec<File>> {
        self.db.lock().unwrap().get_files().map_err(Error::Database)
    }

    pub async fn start_discovery(&self) -> CoreResult<()> {
        if let Some(manager) = &self.bluetooth_manager {
            manager.start_discovery().await.map_err(|e| Error::Bluetooth(error::BluetoothError::DiscoveryFailed { source: Box::new(e) }))?;
        }
        Ok(())
    }

    pub async fn stop_discovery(&self) -> CoreResult<()> {
        if let Some(manager) = &self.bluetooth_manager {
            manager.stop_discovery().await.map_err(|e| Error::Bluetooth(error::BluetoothError::DiscoveryFailed { source: Box::new(e) }))?;
        }
        Ok(())
    }

    pub fn get_discovered_devices(&self) -> Vec<std::sync::Arc<dyn bluebeam_bluetooth::device::Device>> {
        if let Some(manager) = &self.bluetooth_manager {
            manager.discovered_devices()
        } else {
            vec![]
        }
    }

    pub async fn initiate_pairing(&self, device: std::sync::Arc<dyn bluebeam_bluetooth::device::Device>) -> CoreResult<PairingSession> {
        if let Some(manager) = &self.bluetooth_manager {
            let session = PairingSession::initiate(device, manager.as_ref()).await
                .map_err(|e| Error::Bluetooth(error::BluetoothError::PairingFailed { source: Box::new(e) }))?;
            Ok(session)
        } else {
            Err(Error::Bluetooth(error::BluetoothError::NotSupported))
        }
    }

    pub async fn complete_pairing(&self, session: &PairingSession, verified: bool) -> CoreResult<()> {
        if verified {
            session.complete_pairing().await
                .map_err(|e| Error::Bluetooth(error::BluetoothError::PairingFailed { source: Box::new(e) }))?;
            let (id, name, address) = session.get_device_info();
            let fingerprint = session.get_fingerprint_for_storage().unwrap_or_default();
            let device = Device {
                id,
                name,
                address,
                trusted: true,
                last_seen: Utc::now(),
                fingerprint,
            };
            self.add_device(&device)?;
            // Add to trusted devices in settings
            let mut settings = self.settings.lock().unwrap();
            settings.update_settings(|s| {
                s.trusted_devices.push(id.clone());
            }).map_err(Error::Settings)?;
        }
        Ok(())
    }

    pub fn get_settings(&self) -> bluebeam_settings::AppSettings {
        self.settings.lock().unwrap().get_settings().clone()
    }

    pub fn update_settings<F>(&self, updater: F)
    where
        F: FnOnce(&mut bluebeam_settings::AppSettings),
    {
        self.settings.lock().unwrap().update_settings(updater);
    }
}