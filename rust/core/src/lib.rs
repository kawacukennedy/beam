use bluebeam_database::{Database, DatabaseResult, Device, Message, File, FileStatus};
use bluebeam_bluetooth::{manager::BluetoothManager, pairing::PairingSession};
use std::sync::Mutex;
use chrono::Utc;

pub struct Core {
    db: Mutex<Database>,
    bluetooth_manager: Option<std::sync::Arc<dyn BluetoothManager>>,
}

impl Core {
    pub fn new(db_key: &str, bluetooth_manager: Option<std::sync::Arc<dyn BluetoothManager>>) -> DatabaseResult<Self> {
        let db = Database::new(db_key)?;
        Ok(Core {
            db: Mutex::new(db),
            bluetooth_manager,
        })
    }

    pub fn add_device(&self, device: &Device) -> DatabaseResult<()> {
        self.db.lock().unwrap().add_device(device)
    }

    pub fn get_devices(&self) -> DatabaseResult<Vec<Device>> {
        self.db.lock().unwrap().get_devices()
    }

    pub fn add_message(&self, message: &Message) -> DatabaseResult<()> {
        self.db.lock().unwrap().add_message(message)
    }

    pub fn get_messages(&self, conversation_id: &str) -> DatabaseResult<Vec<Message>> {
        self.db.lock().unwrap().get_messages(conversation_id)
    }

    pub fn add_file(&self, file: &File) -> DatabaseResult<()> {
        self.db.lock().unwrap().add_file(file)
    }

    pub fn update_file_status(&self, id: &str, status: FileStatus) -> DatabaseResult<()> {
        self.db.lock().unwrap().update_file_status(id, status)
    }

    pub fn get_files(&self) -> DatabaseResult<Vec<File>> {
        self.db.lock().unwrap().get_files()
    }

    pub async fn start_discovery(&self) -> Result<(), Box<dyn std::error::Error>> {
        if let Some(manager) = &self.bluetooth_manager {
            manager.start_discovery().await?;
        }
        Ok(())
    }

    pub async fn stop_discovery(&self) -> Result<(), Box<dyn std::error::Error>> {
        if let Some(manager) = &self.bluetooth_manager {
            manager.stop_discovery().await?;
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

    pub async fn initiate_pairing(&self, device: std::sync::Arc<dyn bluebeam_bluetooth::device::Device>) -> Result<PairingSession, Box<dyn std::error::Error>> {
        if let Some(manager) = &self.bluetooth_manager {
            let session = PairingSession::initiate(device, manager.as_ref()).await?;
            Ok(session)
        } else {
            Err("No Bluetooth manager".into())
        }
    }

    pub async fn complete_pairing(&self, session: &PairingSession, verified: bool) -> Result<(), Box<dyn std::error::Error>> {
        if verified {
            session.complete_pairing().await?;
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
        }
        Ok(())
    }
}