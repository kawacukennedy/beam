const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const fs = require('fs-extra');
const { v4: uuidv4 } = require('uuid');

const dbPath = path.join(__dirname, '../../data/bluebeam.db');

fs.ensureDirSync(path.dirname(dbPath));

const db = new sqlite3.Database(dbPath);

db.serialize(() => {
  // Devices table
  db.run(`
    CREATE TABLE IF NOT EXISTS devices (
      id TEXT PRIMARY KEY,
      name TEXT NOT NULL,
      bluetooth_address TEXT UNIQUE NOT NULL,
      trusted BOOLEAN DEFAULT 0,
      last_seen DATETIME DEFAULT CURRENT_TIMESTAMP,
      fingerprint TEXT
    )
  `);

  // Messages table
  db.run(`
    CREATE TABLE IF NOT EXISTS messages (
      id TEXT PRIMARY KEY,
      conversation_id TEXT,
      sender_id TEXT,
      receiver_id TEXT,
      content TEXT NOT NULL,
      timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
      status TEXT DEFAULT 'sent',
      FOREIGN KEY (sender_id) REFERENCES devices (id),
      FOREIGN KEY (receiver_id) REFERENCES devices (id)
    )
  `);

  // Files table
  db.run(`
    CREATE TABLE IF NOT EXISTS files (
      id TEXT PRIMARY KEY,
      sender_id TEXT,
      receiver_id TEXT,
      filename TEXT NOT NULL,
      size BIGINT NOT NULL,
      checksum TEXT NOT NULL,
      path TEXT NOT NULL,
      timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
      status TEXT DEFAULT 'pending'
    )
  `);

  // Indexes
  db.run(`CREATE INDEX IF NOT EXISTS idx_devices_addr ON devices(bluetooth_address)`);
  db.run(`CREATE INDEX IF NOT EXISTS idx_messages_conv ON messages(conversation_id)`);
});

module.exports = db;