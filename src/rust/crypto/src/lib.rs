use aes_gcm::{Aes256Gcm, Key, Nonce};
use aes_gcm::aead::{Aead, NewAead};
use curve25519_dalek::scalar::Scalar;
use curve25519_dalek::montgomery::MontgomeryPoint;
use rsa::{RsaPrivateKey, RsaPublicKey, pkcs8::{EncodePrivateKey, EncodePublicKey, DecodePrivateKey, DecodePublicKey}, Pkcs1v15Encrypt};
use sha2::{Sha256, Digest};
use rand::rngs::OsRng;
use std::collections::HashMap;

pub struct CryptoManager {
    private_key: RsaPrivateKey,
    public_key: RsaPublicKey,
    session_keys: HashMap<String, [u8; 32]>,
}

impl CryptoManager {
    pub fn new() -> Self {
        let mut rng = OsRng;
        let private_key = RsaPrivateKey::new(&mut rng, 4096).expect("Failed to generate RSA key");
        let public_key = RsaPublicKey::from(&private_key);

        CryptoManager {
            private_key,
            public_key,
            session_keys: HashMap::new(),
        }
    }

    pub fn get_public_key_pem(&self) -> String {
        self.public_key.to_public_key_pem(Default::default()).unwrap()
    }

    pub fn derive_shared_secret(&self, peer_public_key_pem: &str) -> Result<[u8; 32], Box<dyn std::error::Error>> {
        let peer_public_key = RsaPublicKey::from_public_key_pem(peer_public_key_pem)?;
        
        // For simplicity, use a hash of both public keys as shared secret
        // In real implementation, use proper ECDH
        let mut hasher = Sha256::new();
        hasher.update(self.get_public_key_pem());
        hasher.update(peer_public_key_pem);
        let result = hasher.finalize();
        let mut secret = [0u8; 32];
        secret.copy_from_slice(&result);
        Ok(secret)
    }

    pub fn encrypt_message(&self, shared_secret: &[u8; 32], message: &[u8]) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
        let key = Key::from_slice(shared_secret);
        let cipher = Aes256Gcm::new(key);
        let nonce = Nonce::from_slice(b"unique nonce"); // In production, use unique nonce
        let ciphertext = cipher.encrypt(nonce, message).map_err(|e| e.to_string())?;
        Ok(ciphertext)
    }

    pub fn decrypt_message(&self, shared_secret: &[u8; 32], ciphertext: &[u8]) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
        let key = Key::from_slice(shared_secret);
        let cipher = Aes256Gcm::new(key);
        let nonce = Nonce::from_slice(b"unique nonce");
        let plaintext = cipher.decrypt(nonce, ciphertext).map_err(|e| e.to_string())?;
        Ok(plaintext)
    }

    pub fn calculate_checksum(&self, data: &[u8]) -> String {
        let mut hasher = Sha256::new();
        hasher.update(data);
        format!("{:x}", hasher.finalize())
    }
}

#[no_mangle]
pub extern "C" fn crypto_new() -> *mut CryptoManager {
    Box::into_raw(Box::new(CryptoManager::new()))
}

#[no_mangle]
pub extern "C" fn crypto_free(ptr: *mut CryptoManager) {
    if !ptr.is_null() {
        unsafe { Box::from_raw(ptr); }
    }
}