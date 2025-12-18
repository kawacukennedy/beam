use x25519_dalek::{PublicKey, StaticSecret};
use sha2::{Sha256, Digest};
use rand::rngs::OsRng;
use base64::{Engine as _, engine::general_purpose};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum CryptoError {
    #[error("Key exchange failed")]
    KeyExchange,
    #[error("Invalid key")]
    InvalidKey,
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