const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const isDev = process.env.NODE_ENV === 'development';
const db = require('./src/database/db');
const bluetoothManager = require('./src/bluetooth/bluetoothManager');
const fileTransferManager = require('./src/file_transfer/fileTransfer');
const { v4: uuidv4 } = require('uuid');

function createWindow() {
  const mainWindow = new BrowserWindow({
    width: 1200,
    height: 800,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      enableRemoteModule: false,
      preload: path.join(__dirname, 'preload.js')
    }
  });

  if (isDev) {
    mainWindow.loadURL('http://localhost:3000');
    mainWindow.webContents.openDevTools();
  } else {
    mainWindow.loadFile(path.join(__dirname, 'build/index.html'));
  }
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) {
    createWindow();
  }
});

// IPC handlers for backend functionality
ipcMain.handle('get-devices', async () => {
  try {
    await bluetoothManager.startScanning();
    const discovered = bluetoothManager.getDiscoveredDevices();
    // Also return stored devices
    const stored = await new Promise((resolve, reject) => {
      db.all('SELECT * FROM devices ORDER BY last_seen DESC', (err, rows) => {
        if (err) reject(err);
        else resolve(rows);
      });
    });
    return { discovered, stored };
  } catch (error) {
    console.error('Error getting devices:', error);
    return { discovered: [], stored: [] };
  }
});

ipcMain.handle('add-device', async (event, device) => {
  const id = uuidv4();
  return new Promise((resolve, reject) => {
    db.run(
      'INSERT OR REPLACE INTO devices (id, name, bluetooth_address, last_seen, trusted, fingerprint) VALUES (?, ?, ?, ?, ?, ?)',
      [id, device.name, device.address, new Date().toISOString(), device.trusted || false, device.fingerprint],
      function(err) {
        if (err) reject(err);
        else resolve(id);
      }
    );
  });
});

ipcMain.handle('get-messages', async (event, deviceId) => {
  return new Promise((resolve, reject) => {
    db.all(
      'SELECT * FROM messages WHERE sender_id = ? OR receiver_id = ? ORDER BY timestamp ASC',
      [deviceId, deviceId],
      (err, rows) => {
        if (err) reject(err);
        else resolve(rows);
      }
    );
  });
});

ipcMain.handle('send-message', async (event, message) => {
  const id = uuidv4();
  const conversationId = [message.senderId, message.receiverId].sort().join('-');
  return new Promise((resolve, reject) => {
    db.run(
      'INSERT INTO messages (id, conversation_id, sender_id, receiver_id, content, status) VALUES (?, ?, ?, ?, ?, ?)',
      [id, conversationId, message.senderId, message.receiverId, message.content, 'sent'],
      function(err) {
        if (err) reject(err);
        else resolve(id);
      }
    );
  });
});

ipcMain.handle('send-file', async (event, filePath, deviceId) => {
  const id = uuidv4();
  try {
    const result = await fileTransferManager.sendFile(filePath, deviceId);
    // Log to database
    db.run(
      'INSERT INTO files (id, sender_id, receiver_id, filename, size, checksum, path, status) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
      [id, null, deviceId, result.filename, result.size, result.checksum, filePath, 'in_progress']
    );
    return { ...result, id };
  } catch (error) {
    console.error('Error sending file:', error);
    throw error;
  }
});