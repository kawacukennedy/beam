#![no_main]
use libfuzzer_sys::fuzz_target;

fuzz_target!(|data: &[u8]| {
    let key = b"01234567890123456789012345678901";
    if let Ok(encrypted) = bluebeam_crypto::encrypt_data(key, data) {
        let _ = bluebeam_crypto::decrypt_data(key, &encrypted);
    }
});