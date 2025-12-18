use rusqlite::{Connection, Result};
use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use chrono::{DateTime, Utc};
use uuid::Uuid;
use thiserror::Error;

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

impl From<rusqlite::Error> for DatabaseError {
    fn from(err: rusqlite::Error) -> Self {
        DatabaseError::QueryFailed { source: Box::new(err) }
    }
}

pub type DatabaseResult<T> = Result<T, DatabaseError>;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Device {
    pub id: String,
    pub name: String,
    pub address: String,
    pub trusted: bool,
    pub last_seen: DateTime<Utc>,
    pub fingerprint: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Message {
    pub id: String,
    pub conversation_id: String,
    pub sender_id: String,
    pub receiver_id: String,
    pub content: Vec<u8>,
    pub timestamp: DateTime<Utc>,
    pub status: MessageStatus,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum MessageStatus {
    Sent,
    Delivered,
    Read,
}

impl ToString for MessageStatus {
    fn to_string(&self) -> String {
        match self {
            MessageStatus::Sent => "sent".to_string(),
            MessageStatus::Delivered => "delivered".to_string(),
            MessageStatus::Read => "read".to_string(),
        }
    }
}

impl From<String> for MessageStatus {
    fn from(s: String) -> Self {
        match s.as_str() {
            "delivered" => MessageStatus::Delivered,
            "read" => MessageStatus::Read,
            _ => MessageStatus::Sent,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct File {
    pub id: String,
    pub sender_id: String,
    pub receiver_id: String,
    pub filename: String,
    pub size: i64,
    pub checksum: String,
    pub path: String,
    pub timestamp: DateTime<Utc>,
    pub status: FileStatus,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum FileStatus {
    Pending,
    InProgress,
    Complete,
    Failed,
}

impl ToString for FileStatus {
    fn to_string(&self) -> String {
        match self {
            FileStatus::Pending => "pending".to_string(),
            FileStatus::InProgress => "in_progress".to_string(),
            FileStatus::Complete => "complete".to_string(),
            FileStatus::Failed => "failed".to_string(),
        }
    }
}

impl From<String> for FileStatus {
    fn from(s: String) -> Self {
        match s.as_str() {
            "in_progress" => FileStatus::InProgress,
            "complete" => FileStatus::Complete,
            "failed" => FileStatus::Failed,
            _ => FileStatus::Pending,
        }
    }
}

pub struct Database {
    conn: Connection,
}

impl Database {
    pub fn new(key: &str) -> DatabaseResult<Self> {
        let db_path = Self::get_db_path()?;
        let conn = Connection::open(&db_path)?;

        // Enable SQLCipher encryption
        conn.execute("PRAGMA key = ?;", [key])?;
        conn.execute("PRAGMA cipher_compatibility = 3;", [])?; // For SQLCipher compatibility

        Self::run_migrations(&conn)?;

        Ok(Database { conn })
    }

    fn get_db_path() -> DatabaseResult<PathBuf> {
        let mut data_dir = dirs::data_dir().ok_or(DatabaseError::ConnectionFailed {
            source: Box::new(std::io::Error::new(std::io::ErrorKind::NotFound, "Data directory not found"))
        })?;
        data_dir.push("bluebeam");
        std::fs::create_dir_all(&data_dir)?;
        data_dir.push("bluebeam.db");
        Ok(data_dir)
    }

    fn run_migrations(conn: &Connection) -> DatabaseResult<()> {
        conn.execute(
            "CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY);",
            [],
        )?;

        let current_version: i32 = conn.query_row(
            "SELECT version FROM schema_version ORDER BY version DESC LIMIT 1;",
            [],
            |row| row.get(0),
        ).unwrap_or(0);

        if current_version < 1 {
            Self::migration_1(conn).map_err(|e| DatabaseError::MigrationFailed { source: Box::new(e) })?;
            conn.execute("INSERT INTO schema_version (version) VALUES (1);", [])?;
        }

        Ok(())
    }

    fn migration_1(conn: &Connection) -> DatabaseResult<()> {
        conn.execute(
            "CREATE TABLE devices (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                bluetooth_address TEXT UNIQUE NOT NULL,
                trusted BOOLEAN DEFAULT 0,
                last_seen TEXT NOT NULL,
                fingerprint TEXT
            );",
            [],
        )?;

        conn.execute(
            "CREATE TABLE messages (
                id TEXT PRIMARY KEY,
                conversation_id TEXT,
                sender_id TEXT,
                receiver_id TEXT,
                content BLOB NOT NULL,
                timestamp TEXT NOT NULL,
                status TEXT DEFAULT 'sent' CHECK (status IN ('sent', 'delivered', 'read'))
            );",
            [],
        )?;

        conn.execute(
            "CREATE TABLE files (
                id TEXT PRIMARY KEY,
                sender_id TEXT,
                receiver_id TEXT,
                filename TEXT NOT NULL,
                size BIGINT NOT NULL,
                checksum TEXT NOT NULL,
                path TEXT NOT NULL,
                timestamp TEXT NOT NULL,
                status TEXT DEFAULT 'pending' CHECK (status IN ('pending', 'in_progress', 'complete', 'failed'))
            );",
            [],
        )?;

        conn.execute("CREATE INDEX idx_devices_addr ON devices(bluetooth_address);", [])?;
        conn.execute("CREATE INDEX idx_messages_conv ON messages(conversation_id);", [])?;

        Ok(())
    }

    pub fn add_device(&self, device: &Device) -> DatabaseResult<()> {
        self.conn.execute(
            "INSERT OR REPLACE INTO devices (id, name, bluetooth_address, trusted, last_seen, fingerprint) VALUES (?, ?, ?, ?, ?, ?);",
            (
                &device.id,
                &device.name,
                &device.address,
                device.trusted,
                device.last_seen.to_rfc3339(),
                &device.fingerprint,
            ),
        )?;
        Ok(())
    }

    pub fn get_devices(&self) -> DatabaseResult<Vec<Device>> {
        let mut stmt = self.conn.prepare(
            "SELECT id, name, bluetooth_address, trusted, last_seen, fingerprint FROM devices ORDER BY last_seen DESC;",
        )?;
        let device_iter = stmt.query_map([], |row| {
            Ok(Device {
                id: row.get(0)?,
                name: row.get(1)?,
                address: row.get(2)?,
                trusted: row.get(3)?,
                last_seen: DateTime::parse_from_rfc3339(&row.get::<_, String>(4)?)
                    .map_err(|e| rusqlite::Error::FromSqlConversionFailure(4, rusqlite::types::Type::Text, Box::new(e)))?
                    .with_timezone(&Utc),
                fingerprint: row.get(5)?,
            })
        })?;
        let mut devices = Vec::new();
        for device in device_iter {
            devices.push(device?);
        }
        Ok(devices)
    }

    pub fn add_message(&self, message: &Message) -> DatabaseResult<()> {
        self.conn.execute(
            "INSERT INTO messages (id, conversation_id, sender_id, receiver_id, content, timestamp, status) VALUES (?, ?, ?, ?, ?, ?, ?);",
            (
                &message.id,
                &message.conversation_id,
                &message.sender_id,
                &message.receiver_id,
                &message.content,
                message.timestamp.to_rfc3339(),
                message.status.to_string(),
            ),
        )?;
        Ok(())
    }

    pub fn get_messages(&self, conversation_id: &str) -> DatabaseResult<Vec<Message>> {
        let mut stmt = self.conn.prepare(
            "SELECT id, conversation_id, sender_id, receiver_id, content, timestamp, status FROM messages WHERE conversation_id = ? ORDER BY timestamp ASC;",
        )?;
        let message_iter = stmt.query_map([conversation_id], |row| {
            Ok(Message {
                id: row.get(0)?,
                conversation_id: row.get(1)?,
                sender_id: row.get(2)?,
                receiver_id: row.get(3)?,
                content: row.get(4)?,
                timestamp: DateTime::parse_from_rfc3339(&row.get::<_, String>(5)?)
                    .map_err(|e| rusqlite::Error::FromSqlConversionFailure(5, rusqlite::types::Type::Text, Box::new(e)))?
                    .with_timezone(&Utc),
                status: MessageStatus::from(row.get::<_, String>(6)?),
            })
        })?;
        let mut messages = Vec::new();
        for message in message_iter {
            messages.push(message?);
        }
        Ok(messages)
    }

    pub fn add_file(&self, file: &File) -> DatabaseResult<()> {
        self.conn.execute(
            "INSERT INTO files (id, sender_id, receiver_id, filename, size, checksum, path, timestamp, status) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);",
            (
                &file.id,
                &file.sender_id,
                &file.receiver_id,
                &file.filename,
                file.size,
                &file.checksum,
                &file.path,
                file.timestamp.to_rfc3339(),
                file.status.to_string(),
            ),
        )?;
        Ok(())
    }

    pub fn update_file_status(&self, id: &str, status: FileStatus) -> DatabaseResult<()> {
        self.conn.execute(
            "UPDATE files SET status = ? WHERE id = ?;",
            (status.to_string(), id),
        )?;
        Ok(())
    }

    pub fn get_files(&self) -> DatabaseResult<Vec<File>> {
        let mut stmt = self.conn.prepare(
            "SELECT id, sender_id, receiver_id, filename, size, checksum, path, timestamp, status FROM files ORDER BY timestamp DESC;",
        )?;
        let file_iter = stmt.query_map([], |row| {
            Ok(File {
                id: row.get(0)?,
                sender_id: row.get(1)?,
                receiver_id: row.get(2)?,
                filename: row.get(3)?,
                size: row.get(4)?,
                checksum: row.get(5)?,
                path: row.get(6)?,
                timestamp: DateTime::parse_from_rfc3339(&row.get::<_, String>(7)?)
                    .map_err(|e| rusqlite::Error::FromSqlConversionFailure(7, rusqlite::types::Type::Text, Box::new(e)))?
                    .with_timezone(&Utc),
                status: FileStatus::from(row.get::<_, String>(8)?),
            })
        })?;
        let mut files = Vec::new();
        for file in file_iter {
            files.push(file?);
        }
        Ok(files)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;
    use std::fs;

    fn create_temp_db() -> Database {
        let temp_dir = TempDir::new().unwrap();
        let db_path = temp_dir.path().join("test.db");
        // For testing, use a simple key
        let conn = Connection::open(&db_path).unwrap();
        conn.execute("PRAGMA key = 'test_key';", []).unwrap();
        Self::run_migrations(&conn).unwrap();
        Database { conn }
    }

    #[test]
    fn test_device_serialization() {
        let device = Device {
            id: "test_id".to_string(),
            name: "Test Device".to_string(),
            address: "00:11:22:33:44:55".to_string(),
            trusted: true,
            last_seen: Utc::now(),
            fingerprint: "abc123".to_string(),
        };

        let json = serde_json::to_string(&device).unwrap();
        let deserialized: Device = serde_json::from_str(&json).unwrap();

        assert_eq!(device.id, deserialized.id);
        assert_eq!(device.name, deserialized.name);
        assert_eq!(device.address, deserialized.address);
        assert_eq!(device.trusted, deserialized.trusted);
        assert_eq!(device.fingerprint, deserialized.fingerprint);
    }

    #[test]
    fn test_message_serialization() {
        let message = Message {
            id: Uuid::new_v4().to_string(),
            conversation_id: "conv1".to_string(),
            sender_id: "sender".to_string(),
            receiver_id: "receiver".to_string(),
            content: vec![1, 2, 3, 4],
            timestamp: Utc::now(),
            status: MessageStatus::Sent,
        };

        let json = serde_json::to_string(&message).unwrap();
        let deserialized: Message = serde_json::from_str(&json).unwrap();

        assert_eq!(message.id, deserialized.id);
        assert_eq!(message.conversation_id, deserialized.conversation_id);
        assert_eq!(message.sender_id, deserialized.sender_id);
        assert_eq!(message.receiver_id, deserialized.receiver_id);
        assert_eq!(message.content, deserialized.content);
        assert_eq!(message.status, deserialized.status);
    }

    #[test]
    fn test_file_serialization() {
        let file = File {
            id: Uuid::new_v4().to_string(),
            sender_id: "sender".to_string(),
            receiver_id: "receiver".to_string(),
            filename: "test.txt".to_string(),
            size: 1024,
            checksum: "checksum123".to_string(),
            path: "/path/to/file".to_string(),
            timestamp: Utc::now(),
            status: FileStatus::Pending,
        };

        let json = serde_json::to_string(&file).unwrap();
        let deserialized: File = serde_json::from_str(&json).unwrap();

        assert_eq!(file.id, deserialized.id);
        assert_eq!(file.sender_id, deserialized.sender_id);
        assert_eq!(file.receiver_id, deserialized.receiver_id);
        assert_eq!(file.filename, deserialized.filename);
        assert_eq!(file.size, deserialized.size);
        assert_eq!(file.checksum, deserialized.checksum);
        assert_eq!(file.path, deserialized.path);
        assert_eq!(file.status, deserialized.status);
    }

    #[test]
    fn test_message_status_to_string() {
        assert_eq!(MessageStatus::Sent.to_string(), "sent");
        assert_eq!(MessageStatus::Delivered.to_string(), "delivered");
        assert_eq!(MessageStatus::Read.to_string(), "read");
    }

    #[test]
    fn test_message_status_from_string() {
        assert_eq!(MessageStatus::from("sent".to_string()), MessageStatus::Sent);
        assert_eq!(MessageStatus::from("delivered".to_string()), MessageStatus::Delivered);
        assert_eq!(MessageStatus::from("read".to_string()), MessageStatus::Read);
        assert_eq!(MessageStatus::from("unknown".to_string()), MessageStatus::Sent); // default
    }

    #[test]
    fn test_file_status_to_string() {
        assert_eq!(FileStatus::Pending.to_string(), "pending");
        assert_eq!(FileStatus::InProgress.to_string(), "in_progress");
        assert_eq!(FileStatus::Complete.to_string(), "complete");
        assert_eq!(FileStatus::Failed.to_string(), "failed");
    }

    #[test]
    fn test_file_status_from_string() {
        assert_eq!(FileStatus::from("pending".to_string()), FileStatus::Pending);
        assert_eq!(FileStatus::from("in_progress".to_string()), FileStatus::InProgress);
        assert_eq!(FileStatus::from("complete".to_string()), FileStatus::Complete);
        assert_eq!(FileStatus::from("failed".to_string()), FileStatus::Failed);
        assert_eq!(FileStatus::from("unknown".to_string()), FileStatus::Pending); // default
    }

    #[test]
    fn test_add_and_get_devices() {
        let db = create_temp_db();
        let device = Device {
            id: "device1".to_string(),
            name: "Device 1".to_string(),
            address: "AA:BB:CC:DD:EE:FF".to_string(),
            trusted: true,
            last_seen: Utc::now(),
            fingerprint: "fp1".to_string(),
        };

        db.add_device(&device).unwrap();
        let devices = db.get_devices().unwrap();

        assert_eq!(devices.len(), 1);
        assert_eq!(devices[0].id, device.id);
        assert_eq!(devices[0].name, device.name);
        assert_eq!(devices[0].address, device.address);
        assert_eq!(devices[0].trusted, device.trusted);
        assert_eq!(devices[0].fingerprint, device.fingerprint);
    }

    #[test]
    fn test_add_and_get_messages() {
        let db = create_temp_db();
        let message = Message {
            id: Uuid::new_v4().to_string(),
            conversation_id: "conv1".to_string(),
            sender_id: "sender1".to_string(),
            receiver_id: "receiver1".to_string(),
            content: b"Hello".to_vec(),
            timestamp: Utc::now(),
            status: MessageStatus::Sent,
        };

        db.add_message(&message).unwrap();
        let messages = db.get_messages("conv1").unwrap();

        assert_eq!(messages.len(), 1);
        assert_eq!(messages[0].id, message.id);
        assert_eq!(messages[0].conversation_id, message.conversation_id);
        assert_eq!(messages[0].sender_id, message.sender_id);
        assert_eq!(messages[0].receiver_id, message.receiver_id);
        assert_eq!(messages[0].content, message.content);
        assert_eq!(messages[0].status, message.status);
    }

    #[test]
    fn test_add_and_get_files() {
        let db = create_temp_db();
        let file = File {
            id: Uuid::new_v4().to_string(),
            sender_id: "sender1".to_string(),
            receiver_id: "receiver1".to_string(),
            filename: "test.txt".to_string(),
            size: 100,
            checksum: "checksum".to_string(),
            path: "/tmp/test.txt".to_string(),
            timestamp: Utc::now(),
            status: FileStatus::Pending,
        };

        db.add_file(&file).unwrap();
        let files = db.get_files().unwrap();

        assert_eq!(files.len(), 1);
        assert_eq!(files[0].id, file.id);
        assert_eq!(files[0].sender_id, file.sender_id);
        assert_eq!(files[0].receiver_id, file.receiver_id);
        assert_eq!(files[0].filename, file.filename);
        assert_eq!(files[0].size, file.size);
        assert_eq!(files[0].checksum, file.checksum);
        assert_eq!(files[0].path, file.path);
        assert_eq!(files[0].status, file.status);
    }

    #[test]
    fn test_update_file_status() {
        let db = create_temp_db();
        let mut file = File {
            id: Uuid::new_v4().to_string(),
            sender_id: "sender1".to_string(),
            receiver_id: "receiver1".to_string(),
            filename: "test.txt".to_string(),
            size: 100,
            checksum: "checksum".to_string(),
            path: "/tmp/test.txt".to_string(),
            timestamp: Utc::now(),
            status: FileStatus::Pending,
        };

        db.add_file(&file).unwrap();
        db.update_file_status(&file.id, FileStatus::Complete).unwrap();

        let files = db.get_files().unwrap();
        assert_eq!(files[0].status, FileStatus::Complete);
    }
}