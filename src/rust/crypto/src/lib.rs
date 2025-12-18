use sodiumoxide::crypto::aead::aes256gcm;
use sodiumoxide::crypto::scalarmult::curve25519;
use sodiumoxide::crypto::hash::sha256;
use std::collections::HashMap;
use zeroize::Zeroize;

pub struct CryptoManager {
    ecdh_private: curve25519::Scalar,
    ecdh_public: curve25519::GroupElement,
    session_keys: HashMap<String, aes256gcm::Key>,
}

impl CryptoManager {
    pub fn new() -> Self {
        sodiumoxide::init().unwrap();
        let (ecdh_public, ecdh_private) = curve25519::keypair();

        CryptoManager {
            ecdh_private,
            ecdh_public,
            session_keys: HashMap::new(),
        }
    }

    pub fn get_ecdh_public_key(&self) -> &[u8; 32] {
        self.ecdh_public.as_ref()
    }

    pub fn derive_shared_secret(&self, peer_ecdh_public: &[u8; 32]) -> [u8; 32] {
        let peer_public = curve25519::GroupElement::from_slice(peer_ecdh_public).unwrap();
        let shared_point = curve25519::scalarmult(&self.ecdh_private, &peer_public).unwrap();
        let digest = sha256::hash(shared_point.as_ref());
        let mut secret = [0u8; 32];
        secret.copy_from_slice(&digest.0);
        secret
    }

    pub fn encrypt_message(&mut self, session_id: &str, message: &[u8]) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
        let key = self.session_keys.get(session_id).ok_or("No session key")?;
        let nonce = self.generate_nonce(session_id);
        let ciphertext = aes256gcm::seal(message, None, &nonce, key);
        Ok(ciphertext)
    }

    pub fn decrypt_message(&mut self, session_id: &str, ciphertext: &[u8]) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
        let key = self.session_keys.get(session_id).ok_or("No session key")?;
        let nonce = self.generate_nonce(session_id);
        let plaintext = aes256gcm::open(ciphertext, None, &nonce, key).map_err(|_| "Decryption failed")?;
        Ok(plaintext)
    }

    fn generate_nonce(&self, session_id: &str) -> aes256gcm::Nonce {
        let mut input = session_id.as_bytes().to_vec();
        input.extend_from_slice(b"nonce");
        let digest = sha256::hash(&input);
        aes256gcm::Nonce::from_slice(&digest.0[..12]).unwrap()
    }

    pub fn set_session_key(&mut self, session_id: String, key: [u8; 32]) {
        let key = aes256gcm::Key::from_slice(&key).unwrap();
        self.session_keys.insert(session_id, key);
    }

    pub fn calculate_checksum(&self, data: &[u8]) -> String {
        let digest = sha256::hash(data);
        format!("{:x}", digest)
    }
}

impl Drop for CryptoManager {
    fn drop(&mut self) {
        // Zeroize sensitive data
        self.ecdh_private.zeroize();
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
        unsafe { let _ = Box::from_raw(ptr); }
    }
}

#[no_mangle]
pub extern "C" fn crypto_get_ecdh_public_key(ptr: *mut CryptoManager, out: *mut u8) {
    let mgr = unsafe { &*ptr };
    let key = mgr.get_ecdh_public_key();
    unsafe { std::ptr::copy_nonoverlapping(key.as_ptr(), out, 32); }
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