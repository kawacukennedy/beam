use aes_gcm::{Aes256Gcm, Nonce, KeyInit};
use aes_gcm::aead::Aead;
use curve25519_dalek::{scalar::Scalar, montgomery::MontgomeryPoint};
use rsa::{RsaPrivateKey, RsaPublicKey, pkcs8::EncodePublicKey};
use sha2::{Sha256, Digest};
use rand::{rngs::OsRng, RngCore};
use std::collections::HashMap;
use zeroize::Zeroize;

pub struct CryptoManager {
    ecdh_private: Scalar,
    ecdh_public: MontgomeryPoint,
    rsa_private: RsaPrivateKey,
    rsa_public: RsaPublicKey,
    session_keys: HashMap<String, [u8; 32]>,
}

impl CryptoManager {
    pub fn new() -> Self {
        let mut rng = OsRng;
        let mut bytes = [0u8; 32];
        rng.fill_bytes(&mut bytes);
        let ecdh_private = Scalar::from_bytes_mod_order(bytes);
        let ecdh_public = MontgomeryPoint::mul_base(&ecdh_private);
        let rsa_private = RsaPrivateKey::new(&mut rng, 4096).expect("Failed to generate RSA key");
        let rsa_public = RsaPublicKey::from(&rsa_private);

        CryptoManager {
            ecdh_private,
            ecdh_public,
            rsa_private,
            rsa_public,
            session_keys: HashMap::new(),
        }
    }

    pub fn get_ecdh_public_key(&self) -> &[u8; 32] {
        self.ecdh_public.as_bytes()
    }

    pub fn get_rsa_public_key_pem(&self) -> String {
        self.rsa_public.to_public_key_pem(Default::default()).unwrap()
    }

    pub fn derive_shared_secret(&self, peer_ecdh_public: &[u8; 32]) -> [u8; 32] {
        let peer_public = MontgomeryPoint(*peer_ecdh_public);
        let shared_point = peer_public * self.ecdh_private;
        let mut hasher = Sha256::new();
        hasher.update(shared_point.as_bytes());
        let result = hasher.finalize();
        let mut secret = [0u8; 32];
        secret.copy_from_slice(&result);
        secret
    }

    pub fn encrypt_message(&mut self, session_id: &str, message: &[u8]) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
        let key = self.session_keys.get(session_id).ok_or("No session key")?;
        let cipher = Aes256Gcm::new_from_slice(key).map_err(|_| "Invalid key")?;
        let nonce_bytes = self.generate_nonce(session_id);
        let nonce = Nonce::from_slice(&nonce_bytes);
        let ciphertext = cipher.encrypt(nonce, message).map_err(|e| e.to_string())?;
        Ok(ciphertext)
    }

    pub fn decrypt_message(&mut self, session_id: &str, ciphertext: &[u8]) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
        let key = self.session_keys.get(session_id).ok_or("No session key")?;
        let cipher = Aes256Gcm::new_from_slice(key).map_err(|_| "Invalid key")?;
        let nonce_bytes = self.generate_nonce(session_id);
        let nonce = Nonce::from_slice(&nonce_bytes);
        let plaintext = cipher.decrypt(nonce, ciphertext).map_err(|e| e.to_string())?;
        Ok(plaintext)
    }

    fn generate_nonce(&self, session_id: &str) -> [u8; 12] {
        let mut hasher = Sha256::new();
        hasher.update(session_id.as_bytes());
        hasher.update(b"nonce");
        let result = hasher.finalize();
        let mut nonce = [0u8; 12];
        nonce.copy_from_slice(&result[..12]);
        nonce
    }

    pub fn set_session_key(&mut self, session_id: String, key: [u8; 32]) {
        self.session_keys.insert(session_id, key);
    }

    pub fn calculate_checksum(&self, data: &[u8]) -> String {
        let mut hasher = Sha256::new();
        hasher.update(data);
        format!("{:x}", hasher.finalize())
    }
}

impl Drop for CryptoManager {
    fn drop(&mut self) {
        // Zeroize sensitive data
        self.ecdh_private.zeroize();
        // Note: RSA private key does not implement Zeroize; consider manual zeroization if needed
        // self.rsa_private.zeroize();
        for key in self.session_keys.values_mut() {
            key.zeroize();
        }
        self.session_keys.clear();
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

#[no_mangle]
pub extern "C" fn crypto_get_ecdh_public_key(ptr: *mut CryptoManager, out: *mut u8) {
    let mgr = unsafe { &*ptr };
    let key = mgr.get_ecdh_public_key();
    unsafe { std::ptr::copy_nonoverlapping(key.as_ptr(), out, 32); }
}

#[no_mangle]
pub extern "C" fn crypto_get_rsa_public_key_pem(ptr: *mut CryptoManager) -> *mut std::ffi::c_char {
    let mgr = unsafe { &*ptr };
    let pem = mgr.get_rsa_public_key_pem();
    std::ffi::CString::new(pem).unwrap().into_raw()
}

#[no_mangle]
pub extern "C" fn crypto_derive_shared_secret(ptr: *mut CryptoManager, peer_public: *const u8, out: *mut u8) {
    let mgr = unsafe { &*ptr };
    let peer = unsafe { std::slice::from_raw_parts(peer_public, 32) };
    let mut peer_arr = [0u8; 32];
    peer_arr.copy_from_slice(peer);
    let secret = mgr.derive_shared_secret(&peer_arr);
    unsafe { std::ptr::copy_nonoverlapping(secret.as_ptr(), out, 32); }
}

#[no_mangle]
pub extern "C" fn crypto_set_session_key(ptr: *mut CryptoManager, session_id: *const std::ffi::c_char, key: *const u8) {
    let mgr = unsafe { &mut *ptr };
    let session_id = unsafe { std::ffi::CStr::from_ptr(session_id).to_string_lossy().into_owned() };
    let key_slice = unsafe { std::slice::from_raw_parts(key, 32) };
    let mut key_arr = [0u8; 32];
    key_arr.copy_from_slice(key_slice);
    mgr.set_session_key(session_id, key_arr);
}

#[no_mangle]
pub extern "C" fn crypto_encrypt_message(ptr: *mut CryptoManager, session_id: *const std::ffi::c_char, data: *const u8, len: usize, out_len: *mut usize) -> *mut u8 {
    let mgr = unsafe { &mut *ptr };
    let session_id = unsafe { std::ffi::CStr::from_ptr(session_id).to_string_lossy().into_owned() };
    let data_slice = unsafe { std::slice::from_raw_parts(data, len) };
    match mgr.encrypt_message(&session_id, data_slice) {
        Ok(encrypted) => {
            unsafe { *out_len = encrypted.len(); }
            let ptr = encrypted.as_ptr() as *mut u8;
            std::mem::forget(encrypted);
            ptr
        }
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn crypto_decrypt_message(ptr: *mut CryptoManager, session_id: *const std::ffi::c_char, data: *const u8, len: usize, out_len: *mut usize) -> *mut u8 {
    let mgr = unsafe { &mut *ptr };
    let session_id = unsafe { std::ffi::CStr::from_ptr(session_id).to_string_lossy().into_owned() };
    let data_slice = unsafe { std::slice::from_raw_parts(data, len) };
    match mgr.decrypt_message(&session_id, data_slice) {
        Ok(decrypted) => {
            unsafe { *out_len = decrypted.len(); }
            let ptr = decrypted.as_ptr() as *mut u8;
            std::mem::forget(decrypted);
            ptr
        }
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn crypto_calculate_checksum(ptr: *mut CryptoManager, data: *const u8, len: usize) -> *mut std::ffi::c_char {
    let mgr = unsafe { &*ptr };
    let data_slice = unsafe { std::slice::from_raw_parts(data, len) };
    let checksum = mgr.calculate_checksum(data_slice);
    std::ffi::CString::new(checksum).unwrap().into_raw()
}