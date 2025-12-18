#![no_main]
use libfuzzer_sys::fuzz_target;

fuzz_target!(|data: &[u8]| {
    if data.len() == 32 {
        let pk = x25519_dalek::PublicKey::from(<[u8; 32]>::try_from(data).unwrap());
        let _ = bluebeam_crypto::generate_fingerprint(&pk);
    }
});