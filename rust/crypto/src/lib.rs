use x25519_dalek::{PublicKey, StaticSecret};
use sha2::{Sha256, Digest};
use rand::rngs::OsRng;
use base64::{Engine as _, engine::general_purpose};
use thiserror::Error;
use aes_gcm::{Aes256Gcm, Key, Nonce};
use aes_gcm::aead::{Aead, NewAead};

#[derive(Debug, Error)]
pub enum CryptoError {
    #[error("Key exchange failed")]
    KeyExchangeFailed,
    #[error("Invalid key provided")]
    InvalidKey,
    #[error("Encryption failed: {source}")]
    EncryptionFailed { source: Box<dyn std::error::Error + Send + Sync> },
    #[error("Decryption failed: {source}")]
    DecryptionFailed { source: Box<dyn std::error::Error + Send + Sync> },
}

pub type Result<T> = std::result::Result<T, CryptoError>;

pub struct KeyPair {
    pub public_key: PublicKey,
    pub secret_key: StaticSecret,
}

impl KeyPair {
    pub fn generate() -> Self {
        let secret = StaticSecret::random_from_rng(OsRng);
        let public = PublicKey::from(&secret);
        KeyPair {
            public_key: public,
            secret_key: secret,
        }
    }
}

pub fn compute_shared_secret(local_secret: &StaticSecret, remote_public: &PublicKey) -> [u8; 32] {
    local_secret.diffie_hellman(remote_public).to_bytes()
}

pub fn generate_fingerprint(public_key: &PublicKey) -> String {
    let mut hasher = Sha256::new();
    hasher.update(public_key.as_bytes());
    let hash = hasher.finalize();
    // Take first 8 bytes for fingerprint, encode as base64
    let short_hash = &hash[..8];
    general_purpose::STANDARD.encode(short_hash)
}

pub fn generate_pin_from_fingerprint(fingerprint: &str) -> String {
    // Simple PIN generation: take first 6 characters of base64, map to digits
    let chars: Vec<char> = fingerprint.chars().collect();
    let mut pin = String::new();
    for i in 0..6 {
        let c = chars.get(i).unwrap_or(&'A');
        let digit = (c as u32 % 10) as u8 + b'0';
        pin.push(digit as char);
    }
    pin
}

pub fn encrypt_data(key: &[u8; 32], data: &[u8]) -> Result<Vec<u8>> {
    let cipher = Aes256Gcm::new(Key::from_slice(key));
    let nonce = Nonce::from_slice(b"unique nonce"); // In real app, use random nonce
    let ciphertext = cipher.encrypt(nonce, data).map_err(|e| CryptoError::EncryptionFailed { source: Box::new(e) })?;
    Ok(ciphertext)
}

pub fn decrypt_data(key: &[u8; 32], data: &[u8]) -> Result<Vec<u8>> {
    let cipher = Aes256Gcm::new(Key::from_slice(key));
    let nonce = Nonce::from_slice(b"unique nonce");
    let plaintext = cipher.decrypt(nonce, data).map_err(|e| CryptoError::DecryptionFailed { source: Box::new(e) })?;
    // Encrypted storage: data is decrypted only when needed, no plaintext storage
    Ok(plaintext)
}

#[no_mangle]
pub extern "C" fn crypto_encrypt(data: *const u8, len: usize, out: *mut u8) -> i32 {
    let key = b"01234567890123456789012345678901";
    let data_slice = unsafe { std::slice::from_raw_parts(data, len) };
    match encrypt_data(key, data_slice) {
        Ok(enc) => {
            unsafe {
                std::ptr::copy(enc.as_ptr(), out, enc.len());
            }
            enc.len() as i32
        }
        Err(_) => -1,
    }
}

#[no_mangle]
pub extern "C" fn crypto_decrypt(data: *const u8, len: usize, out: *mut u8) -> i32 {
    let key = b"01234567890123456789012345678901";
    let data_slice = unsafe { std::slice::from_raw_parts(data, len) };
    match decrypt_data(key, data_slice) {
        Ok(dec) => {
            unsafe {
                std::ptr::copy(dec.as_ptr(), out, dec.len());
            }
            dec.len() as i32
        }
        Err(_) => -1,
    }
}