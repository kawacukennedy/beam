use std::collections::HashMap;
use std::sync::Arc;
use std::sync::Mutex;
use crossbeam::channel::{unbounded, Receiver, Sender};
use serde::{Deserialize, Serialize};
use uuid::Uuid;
use bluebeam_bluetooth as bt;
use bluebeam_crypto as crypto;
use rusqlite::{Connection, params};
use chrono::{DateTime, Utc};
use std::thread;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum Event {
    // UI events
    StartDiscovery,
    StopDiscovery,
    PairWithDevice(String), // device id
    SendMessage(String, String), // session_id, message
    SendFile(String, String), // session_id, file_path
    CancelTransfer(String), // transfer_id

    // Bluetooth events
    DeviceDiscovered(String, String), // id, name
    DeviceConnected(String),
    DeviceDisconnected(String),
    DataReceived(String, Vec<u8>), // session_id, data

    // Internal
    Shutdown,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum StateUpdate {
    DiscoveryStarted,
    DiscoveryStopped,
    DeviceFound { id: String, name: String },
    DevicePaired { id: String },
    MessageReceived { session_id: String, message: String, timestamp: DateTime<Utc> },
    FileTransferProgress { transfer_id: String, progress: f64 },
    FileReceived { session_id: String, file_path: String },
    Error { code: ErrorCode, message: String },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ErrorCode {
    BluetoothError,
    CryptoError,
    NetworkError,
    FileError,
    SessionError,
}

pub struct CoreManager {
    event_tx: Sender<Event>,
    state_rx: Receiver<StateUpdate>,
    _handle: thread::JoinHandle<()>,
    db: Arc<Mutex<Connection>>,
    crypto_mgr: Arc<Mutex<crypto::CryptoManager>>,
    bt_mgr: Arc<Mutex<bt::BluetoothManager>>,
    sessions: Arc<Mutex<HashMap<String, Session>>>,
    transfers: Arc<Mutex<HashMap<String, FileTransfer>>>,
}

#[derive(Debug)]
struct Session {
    id: String,
    device_id: String,
    state: SessionState,
    crypto_session_id: String,
}

#[derive(Debug)]
enum SessionState {
    Pairing,
    Active,
    Closed,
}

#[derive(Debug)]
struct FileTransfer {
    id: String,
    session_id: String,
    file_path: String,
    total_size: u64,
    sent_size: u64,
    state: TransferState,
}

#[derive(Debug)]
enum TransferState {
    Pending,
    InProgress,
    Paused,
    Completed,
    Failed,
}

impl CoreManager {
    pub fn new() -> Result<Self, Box<dyn std::error::Error>> {
        let (event_tx, event_rx) = unbounded();
        let (state_tx, state_rx) = unbounded();

        let db = Connection::open_in_memory()?; // or file path
        init_db(&db)?;

        let crypto_mgr = Arc::new(Mutex::new(crypto::CryptoManager::new()));
        let bt_mgr = Arc::new(Mutex::new(bt::BluetoothManager::new()));
        let sessions = Arc::new(Mutex::new(HashMap::new()));
        let transfers = Arc::new(Mutex::new(HashMap::new()));

        let sessions_clone = sessions.clone();
        let transfers_clone = transfers.clone();
        let crypto_mgr_clone = crypto_mgr.clone();
        let bt_mgr_clone = bt_mgr.clone();
        let db_clone = db.clone();

        let handle = thread::spawn(move || {
            Self::event_loop(event_rx, state_tx, sessions_clone, transfers_clone, crypto_mgr_clone, bt_mgr_clone, db_clone);
        });

        let manager = CoreManager {
            event_tx,
            state_rx,
            _handle: handle,
            db: Arc::new(Mutex::new(db)),
            crypto_mgr,
            bt_mgr,
            sessions,
            transfers,
        };

        Ok(manager)
    }

    fn event_loop(
        event_rx: Receiver<Event>,
        state_tx: Sender<StateUpdate>,
        sessions: Arc<Mutex<HashMap<String, Session>>>,
        transfers: Arc<Mutex<HashMap<String, FileTransfer>>>,
        crypto_mgr: Arc<Mutex<crypto::CryptoManager>>,
        bt_mgr: Arc<Mutex<bt::BluetoothManager>>,
        db: Arc<Mutex<Connection>>,
    ) {
        while let Ok(event) = event_rx.recv() {
            match event {
                Event::StartDiscovery => {
                    Self::handle_start_discovery(&state_tx, &bt_mgr);
                }
                Event::StopDiscovery => {
                    Self::handle_stop_discovery(&state_tx, &bt_mgr);
                }
                Event::PairWithDevice(device_id) => {
                    Self::handle_pair_with_device(device_id, &state_tx, &sessions, &crypto_mgr, &bt_mgr);
                }
                Event::SendMessage(session_id, message) => {
                    Self::handle_send_message(session_id, message, &state_tx, &sessions, &crypto_mgr, &bt_mgr, &db);
                }
                Event::SendFile(session_id, file_path) => {
                    Self::handle_send_file(session_id, file_path, &state_tx, &sessions, &transfers, &crypto_mgr, &bt_mgr, &db);
                }
                Event::CancelTransfer(transfer_id) => {
                    Self::handle_cancel_transfer(transfer_id, &state_tx, &transfers);
                }
                Event::DeviceDiscovered(id, name) => {
                    Self::handle_device_discovered(id, name, &state_tx);
                }
                Event::DeviceConnected(id) => {
                    Self::handle_device_connected(id, &sessions);
                }
                Event::DeviceDisconnected(id) => {
                    Self::handle_device_disconnected(id, &sessions);
                }
                Event::DataReceived(session_id, data) => {
                    Self::handle_data_received(session_id, data, &state_tx, &sessions, &transfers, &crypto_mgr, &db);
                }
                Event::Shutdown => break,
            }
        }
    }

    fn handle_start_discovery(state_tx: &Sender<StateUpdate>, bt_mgr: &Arc<Mutex<bt::BluetoothManager>>) {
        // Call bt_mgr to start discovery
        // For now, placeholder
        let _ = state_tx.send(StateUpdate::DiscoveryStarted);
    }

    fn handle_stop_discovery(state_tx: &Sender<StateUpdate>, bt_mgr: &Arc<Mutex<bt::BluetoothManager>>) {
        // Stop discovery
        let _ = state_tx.send(StateUpdate::DiscoveryStopped);
    }

    fn handle_pair_with_device(
        device_id: String,
        state_tx: &Sender<StateUpdate>,
        sessions: &Arc<Mutex<HashMap<String, Session>>>,
        crypto_mgr: &Arc<Mutex<crypto::CryptoManager>>,
        bt_mgr: &Arc<Mutex<bt::BluetoothManager>>,
    ) {
        // Start pairing process
        // Generate keys, send public key, etc.
        // Placeholder
        let session_id = Uuid::new_v4().to_string();
        let session = Session {
            id: session_id.clone(),
            device_id: device_id.clone(),
            state: SessionState::Pairing,
            crypto_session_id: session_id.clone(),
        };
        sessions.lock().unwrap().insert(session_id.clone(), session);
        let _ = state_tx.send(StateUpdate::DevicePaired { id: device_id });
    }

    fn handle_send_message(
        session_id: String,
        message: String,
        state_tx: &Sender<StateUpdate>,
        sessions: &Arc<Mutex<HashMap<String, Session>>>,
        crypto_mgr: &Arc<Mutex<crypto::CryptoManager>>,
        bt_mgr: &Arc<Mutex<bt::BluetoothManager>>,
        db: &Arc<Mutex<Connection>>,
    ) {
        // Encrypt and send via bt
        // Placeholder
    }

    fn handle_send_file(
        session_id: String,
        file_path: String,
        state_tx: &Sender<StateUpdate>,
        sessions: &Arc<Mutex<HashMap<String, Session>>>,
        transfers: &Arc<Mutex<HashMap<String, FileTransfer>>>,
        crypto_mgr: &Arc<Mutex<crypto::CryptoManager>>,
        bt_mgr: &Arc<Mutex<bt::BluetoothManager>>,
        db: &Arc<Mutex<Connection>>,
    ) {
        // Start file transfer
        // Placeholder
    }

    fn handle_cancel_transfer(
        transfer_id: String,
        state_tx: &Sender<StateUpdate>,
        transfers: &Arc<Mutex<HashMap<String, FileTransfer>>>,
    ) {
        // Cancel transfer
        // Placeholder
    }

    fn handle_device_discovered(id: String, name: String, state_tx: &Sender<StateUpdate>) {
        let _ = state_tx.send(StateUpdate::DeviceFound { id, name });
    }

    fn handle_device_connected(id: String, sessions: &Arc<Mutex<HashMap<String, Session>>>) {
        // Update session state
    }

    fn handle_device_disconnected(id: String, sessions: &Arc<Mutex<HashMap<String, Session>>>) {
        // Close session
    }

    fn handle_data_received(
        session_id: String,
        data: Vec<u8>,
        state_tx: &Sender<StateUpdate>,
        sessions: &Arc<Mutex<HashMap<String, Session>>>,
        transfers: &Arc<Mutex<HashMap<String, FileTransfer>>>,
        crypto_mgr: &Arc<Mutex<crypto::CryptoManager>>,
        db: &Arc<Mutex<Connection>>,
    ) {
        // Decrypt and process
        // If message, emit MessageReceived
        // If file chunk, update transfer
    }

    pub fn send_event(&self, event: Event) {
        let _ = self.event_tx.send(event);
    }

    pub fn try_recv_state_update(&self) -> Option<StateUpdate> {
        self.state_rx.try_recv().ok()
    }
}

fn init_db(conn: &Connection) -> Result<(), rusqlite::Error> {
    conn.execute(
        "CREATE TABLE messages (
            id INTEGER PRIMARY KEY,
            session_id TEXT NOT NULL,
            content TEXT NOT NULL,
            timestamp DATETIME NOT NULL
        )",
        [],
    )?;
    conn.execute(
        "CREATE TABLE file_transfers (
            id TEXT PRIMARY KEY,
            session_id TEXT NOT NULL,
            file_path TEXT NOT NULL,
            total_size INTEGER NOT NULL,
            sent_size INTEGER NOT NULL,
            state TEXT NOT NULL
        )",
        [],
    )?;
    Ok(())
}

// FFI functions
#[no_mangle]
pub extern "C" fn core_new() -> *mut CoreManager {
    match CoreManager::new() {
        Ok(mgr) => Box::into_raw(Box::new(mgr)),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn core_free(ptr: *mut CoreManager) {
    if !ptr.is_null() {
        unsafe {
            let mgr = Box::from_raw(ptr);
            // Send shutdown
            let _ = mgr.event_tx.send(Event::Shutdown);
            // Wait for thread? But can't in FFI
        }
    }
}

#[no_mangle]
pub extern "C" fn core_send_event(ptr: *mut CoreManager, event_json: *const std::ffi::c_char) {
    if ptr.is_null() || event_json.is_null() {
        return;
    }
    let mgr = unsafe { &*ptr };
    let event_str = unsafe { std::ffi::CStr::from_ptr(event_json).to_string_lossy() };
    if let Ok(event) = serde_json::from_str(&event_str) {
        mgr.send_event(event);
    }
}

#[no_mangle]
pub extern "C" fn core_recv_update(ptr: *mut CoreManager) -> *mut std::ffi::c_char {
    if ptr.is_null() {
        return std::ptr::null_mut();
    }
    let mgr = unsafe { &*ptr };
    if let Some(update) = mgr.try_recv_state_update() {
        if let Ok(json) = serde_json::to_string(&update) {
            std::ffi::CString::new(json).unwrap().into_raw()
        } else {
            std::ptr::null_mut()
        }
    } else {
        std::ptr::null_mut()
    }
}

// Free the string returned by recv_update
#[no_mangle]
pub extern "C" fn core_free_string(ptr: *mut std::ffi::c_char) {
    if !ptr.is_null() {
        unsafe { let _ = std::ffi::CString::from_raw(ptr); }
    }
}