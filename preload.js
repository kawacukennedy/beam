const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  getDevices: () => ipcRenderer.invoke('get-devices'),
  addDevice: (device) => ipcRenderer.invoke('add-device', device),
  getMessages: (deviceId) => ipcRenderer.invoke('get-messages', deviceId),
  sendMessage: (message) => ipcRenderer.invoke('send-message', message),
  sendFile: (filePath, deviceId) => ipcRenderer.invoke('send-file', filePath, deviceId),
});