const fs = require('fs-extra');
const crypto = require('crypto');
const path = require('path');

class FileTransferManager {
  constructor() {
    this.chunkSize = 64 * 1024; // 64KB
  }

  async sendFile(filePath, deviceId) {
    const stats = await fs.stat(filePath);
    const fileSize = stats.size;
    const filename = path.basename(filePath);
    const checksum = await this.calculateChecksum(filePath);

    // Log to database
    // TODO: Insert into files table

    const chunks = Math.ceil(fileSize / this.chunkSize);
    for (let i = 0; i < chunks; i++) {
      const start = i * this.chunkSize;
      const end = Math.min(start + this.chunkSize, fileSize);
      const chunk = await fs.readFile(filePath, { start, end: end - 1 });

      // TODO: Send chunk over Bluetooth with encryption
      console.log(`Sending chunk ${i + 1}/${chunks} of ${filename}`);
    }

    return { filename, size: fileSize, checksum };
  }

  async receiveFile(metadata, chunks) {
    const downloadDir = path.join(require('os').homedir(), 'Downloads', 'BlueBeam');
    await fs.ensureDir(downloadDir);
    const filePath = path.join(downloadDir, metadata.filename);

    const writeStream = fs.createWriteStream(filePath);
    for (const chunk of chunks) {
      writeStream.write(chunk);
    }
    writeStream.end();

    // Verify checksum
    const receivedChecksum = await this.calculateChecksum(filePath);
    if (receivedChecksum !== metadata.checksum) {
      await fs.unlink(filePath);
      throw new Error('Checksum mismatch');
    }

    return filePath;
  }

  async calculateChecksum(filePath) {
    const fileBuffer = await fs.readFile(filePath);
    return crypto.createHash('sha256').update(fileBuffer).digest('hex');
  }
}

module.exports = new FileTransferManager();