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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_error_conversion() {
        let crypto_err = bluebeam_crypto::CryptoError::InvalidKey;
        let core_err: Error = crypto_err.into();
        match core_err {
            Error::Crypto(CryptoError::InvalidKey) => {},
            _ => panic!("Wrong error type"),
        }
        assert_eq!(core_err.error_code(), ErrorCode::CryptoInvalidKey);
        assert_eq!(core_err.user_message(), "Invalid encryption key detected. Your security may be compromised.");
    }

    #[test]
    fn test_error_codes() {
        assert_eq!(ErrorCode::Unknown as u32, 1000);
        assert_eq!(ErrorCode::DatabaseConnectionFailed as u32, 2000);
        assert_eq!(ErrorCode::BluetoothNotSupported as u32, 3000);
        assert_eq!(ErrorCode::CryptoKeyExchangeFailed as u32, 4000);
        assert_eq!(ErrorCode::SettingsLoadFailed as u32, 5000);
    }
}

#[cfg(test)]
mod integration_tests {
    use super::*;
    use bluebeam_bluetooth::{device::Device, connection::Connection, transport::Transport, manager::BluetoothManager};
    use std::sync::Arc;
    use tokio::sync::Mutex;
    use std::collections::HashMap;
    use x25519_dalek::PublicKey;

    // Mock implementations for testing
    struct MockDevice {
        id: String,
        name: Option<String>,
        address: String,
    }

    impl MockDevice {
        fn new(id: String, name: Option<String>, address: String) -> Self {
            MockDevice { id, name, address }
        }
    }

    #[async_trait::async_trait]
    impl Device for MockDevice {
        fn id(&self) -> String { self.id.clone() }
        fn name(&self) -> Option<String> { self.name.clone() }
        fn address(&self) -> String { self.address.clone() }
    }

    struct MockTransport {
        sender: tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
        receiver: tokio::sync::mpsc::UnboundedReceiver<Vec<u8>>,
        inject_sender: tokio::sync::mpsc::UnboundedSender<Vec<u8>>,
    }

    impl MockTransport {
        fn new() -> (Self, tokio::sync::mpsc::UnboundedSender<Vec<u8>>) {
            let (tx1, rx1) = tokio::sync::mpsc::unbounded_channel();
            let (tx2, rx2) = tokio::sync::mpsc::unbounded_channel();
            (MockTransport { sender: tx1, receiver: rx2, inject_sender: tx2 }, rx1)
        }

        fn inject_data(&self, data: Vec<u8>) {
            self.inject_sender.send(data).unwrap();
        }
    }

    #[async_trait::async_trait]
    impl Transport for MockTransport {
        async fn send(&self, data: &[u8]) -> bluebeam_bluetooth::Result<()> {
            self.sender.send(data.to_vec()).unwrap();
            Ok(())
        }

        async fn receive(&self) -> bluebeam_bluetooth::Result<Vec<u8>> {
            match self.receiver.recv().await {
                Some(data) => Ok(data),
                None => Ok(vec![]),
            }
        }
    }

    struct MockConnection {
        transport: Arc<dyn Transport>,
    }

    impl MockConnection {
        fn new(transport: Arc<dyn Transport>) -> Self {
            MockConnection { transport }
        }
    }

    #[async_trait::async_trait]
    impl Connection for MockConnection {
        async fn create_transport(&self, _service_uuid: uuid::Uuid) -> bluebeam_bluetooth::Result<Arc<dyn Transport>> {
            Ok(self.transport.clone())
        }
    }

    struct MockBluetoothManager {
        devices: Vec<Arc<dyn Device>>,
        connections: Arc<Mutex<HashMap<String, Arc<dyn Connection>>>>,
        inject_senders: Arc<Mutex<HashMap<String, tokio::sync::mpsc::UnboundedSender<Vec<u8>>>>>,
    }

    impl MockBluetoothManager {
        fn new() -> Self {
            MockBluetoothManager {
                devices: vec![],
                connections: Arc::new(Mutex::new(HashMap::new())),
                inject_senders: Arc::new(Mutex::new(HashMap::new())),
            }
        }

        fn add_device(&mut self, device: Arc<dyn Device>) {
            self.devices.push(device);
        }

        fn get_inject_sender(&self, device_id: &str) -> Option<tokio::sync::mpsc::UnboundedSender<Vec<u8>>> {
            let senders = self.inject_senders.try_lock().ok()?;
            senders.get(device_id).cloned()
        }
    }

    #[async_trait::async_trait]
    impl BluetoothManager for MockBluetoothManager {
        async fn start_discovery(&self) -> bluebeam_bluetooth::Result<()> {
            Ok(())
        }

        async fn stop_discovery(&self) -> bluebeam_bluetooth::Result<()> {
            Ok(())
        }

        fn discovered_devices(&self) -> Vec<Arc<dyn Device>> {
            self.devices.clone()
        }

        async fn connect(&self, device: &dyn Device) -> bluebeam_bluetooth::Result<Arc<dyn Connection>> {
            let (transport, inject_rx) = MockTransport::new();
            let connection = Arc::new(MockConnection::new(Arc::new(transport)));
            let mut conns = self.connections.lock().await;
            conns.insert(device.id(), connection.clone());
            let mut senders = self.inject_senders.lock().await;
            senders.insert(device.id(), inject_rx);
            Ok(connection)
        }
    }

    #[tokio::test]
    async fn test_pairing_integration() {
        let temp_dir = tempfile::TempDir::new().unwrap();
        let db_path = temp_dir.path().join("test.db");
        let db = bluebeam_database::Database::new("test_key").unwrap();
        let settings = bluebeam_settings::SettingsManager::new().unwrap();

        let mut mock_manager = MockBluetoothManager::new();
        let device = Arc::new(MockDevice::new("test_device".to_string(), Some("Test Device".to_string()), "00:11:22:33:44:55".to_string()));
        mock_manager.add_device(device.clone());

        let core = Core {
            db: Mutex::new(db),
            bluetooth_manager: Some(Arc::new(mock_manager)),
            settings: Mutex::new(settings),
        };

        // Test discovery
        core.start_discovery().await.unwrap();
        let devices = core.get_discovered_devices();
        assert_eq!(devices.len(), 1);
        assert_eq!(devices[0].id(), "test_device");

        // Test initiate pairing
        let session = core.initiate_pairing(device).await.unwrap();
        assert_eq!(session.get_device_info().0, "test_device");

        // Test exchange keys (mock the remote response)
        let inject_sender = mock_manager.get_inject_sender("test_device").unwrap();
        let remote_key = PublicKey::from([0u8; 32]);
        inject_sender.send(remote_key.as_bytes().to_vec()).unwrap();

        session.exchange_keys().await.unwrap();
        let pin = session.get_pin().unwrap();
        assert!(!pin.is_empty());

        // Test verify pin
        let verified = session.verify_pin(&pin).await.unwrap();
        assert!(verified);

        // Test complete pairing
        inject_sender.send(b"PAIRING_COMPLETE".to_vec()).unwrap();
        session.complete_pairing().await.unwrap();

        // Test complete pairing in core
        core.complete_pairing(&session, true).await.unwrap();

        // Check device added
        let devices = core.get_devices().unwrap();
        assert_eq!(devices.len(), 1);
        assert_eq!(devices[0].id, "test_device");
        assert!(devices[0].trusted);
    }

    #[test]
    fn test_messaging_integration() {
        let db = bluebeam_database::Database::new("test_key").unwrap();
        let settings = bluebeam_settings::SettingsManager::new().unwrap();

        let core = Core {
            db: Mutex::new(db),
            bluetooth_manager: None,
            settings: Mutex::new(settings),
        };

        let message = Message {
            id: "msg1".to_string(),
            conversation_id: "conv1".to_string(),
            sender_id: "sender".to_string(),
            receiver_id: "receiver".to_string(),
            content: b"Hello".to_vec(),
            timestamp: Utc::now(),
            status: MessageStatus::Sent,
        };

        core.add_message(&message).unwrap();

        let messages = core.get_messages("conv1").unwrap();
        assert_eq!(messages.len(), 1);
        assert_eq!(messages[0].id, "msg1");
        assert_eq!(messages[0].content, b"Hello");
    }

    #[test]
    fn test_file_transfer_integration() {
        let db = bluebeam_database::Database::new("test_key").unwrap();
        let settings = bluebeam_settings::SettingsManager::new().unwrap();

        let core = Core {
            db: Mutex::new(db),
            bluetooth_manager: None,
            settings: Mutex::new(settings),
        };

        let file = File {
            id: "file1".to_string(),
            sender_id: "sender".to_string(),
            receiver_id: "receiver".to_string(),
            filename: "test.txt".to_string(),
            size: 100,
            checksum: "checksum".to_string(),
            path: "/tmp/test.txt".to_string(),
            timestamp: Utc::now(),
            status: FileStatus::Pending,
        };

        core.add_file(&file).unwrap();

        let files = core.get_files().unwrap();
        assert_eq!(files.len(), 1);
        assert_eq!(files[0].id, "file1");
        assert_eq!(files[0].status, FileStatus::Pending);

        core.update_file_status("file1", FileStatus::Complete).unwrap();

        let files = core.get_files().unwrap();
        assert_eq!(files[0].status, FileStatus::Complete);
    }
}