#![no_main]
use libfuzzer_sys::fuzz_target;

fuzz_target!(|data: &[u8]| {
    if data.len() == 64 {
        let secret_bytes = <[u8; 32]>::try_from(&data[0..32]).unwrap();
        let public_bytes = <[u8; 32]>::try_from(&data[32..64]).unwrap();
        let secret = x25519_dalek::StaticSecret::from(secret_bytes);
        let public = x25519_dalek::PublicKey::from(public_bytes);
        let _ = bluebeam_crypto::compute_shared_secret(&secret, &public);
    }
});