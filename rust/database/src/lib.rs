use rusqlite::{Connection, Result};
use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use chrono::{DateTime, Utc};
use uuid::Uuid;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum DatabaseError {
    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
    #[error("Dirs error: {0}")]
    Dirs(String),
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
        let mut data_dir = dirs::data_dir().ok_or(DatabaseError::Dirs("Data directory not found".to_string()))?;
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
            Self::migration_1(conn)?;
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