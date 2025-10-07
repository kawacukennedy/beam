const crypto = require('crypto');

class CryptoManager {
  constructor() {
    this.keyPair = crypto.generateKeyPairSync('ec', {
      namedCurve: 'secp256k1',
      publicKeyEncoding: { type: 'spki', format: 'pem' },
      privateKeyEncoding: { type: 'pkcs8', format: 'pem' }
    });
  }

  getPublicKey() {
    return this.keyPair.publicKey;
  }

  async deriveSharedSecret(peerPublicKey) {
    const ecdh = crypto.createECDH('secp256k1');
    ecdh.setPrivateKey(crypto.createPrivateKey(this.keyPair.privateKey));
    const sharedSecret = ecdh.computeSecret(peerPublicKey, 'pem', 'hex');
    return crypto.createHash('sha256').update(sharedSecret, 'hex').digest();
  }

  encryptMessage(sharedSecret, message) {
    const iv = crypto.randomBytes(16);
    const cipher = crypto.createCipher('aes-256-gcm', sharedSecret);
    cipher.setIV(iv);
    let encrypted = cipher.update(JSON.stringify(message), 'utf8', 'hex');
    encrypted += cipher.final('hex');
    const authTag = cipher.getAuthTag();
    return { encrypted, iv: iv.toString('hex'), authTag: authTag.toString('hex') };
  }

  decryptMessage(sharedSecret, encryptedData) {
    const decipher = crypto.createDecipher('aes-256-gcm', sharedSecret);
    decipher.setIV(Buffer.from(encryptedData.iv, 'hex'));
    decipher.setAuthTag(Buffer.from(encryptedData.authTag, 'hex'));
    let decrypted = decipher.update(encryptedData.encrypted, 'hex', 'utf8');
    decrypted += decipher.final('utf8');
    return JSON.parse(decrypted);
  }
}

module.exports = new CryptoManager();