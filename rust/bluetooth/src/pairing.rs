use crate::device::Device;
use crate::connection::Connection;
use crate::transport::Transport;
use crate::Result;
use bluebeam_crypto::{KeyPair, compute_shared_secret, generate_fingerprint, generate_pin_from_fingerprint};
use std::sync::Arc;
use uuid::Uuid;
use tokio::sync::mpsc;

const PAIRING_SERVICE_UUID: &str = "00001101-0000-1000-8000-00805f9b34fb"; // Standard SPP UUID

pub struct PairingSession {
    device: Arc<dyn Device>,
    connection: Arc<dyn Connection>,
    transport: Arc<dyn Transport>,
    local_keypair: KeyPair,
    remote_public_key: Option<x25519_dalek::PublicKey>,
    fingerprint: Option<String>,
    pin: Option<String>,
}

impl PairingSession {
    pub async fn initiate(device: Arc<dyn Device>, manager: &dyn crate::manager::BluetoothManager) -> Result<Self> {
        let connection = manager.connect(device.as_ref()).await?;
        let service_uuid = Uuid::parse_str(PAIRING_SERVICE_UUID).unwrap();
        let transport = connection.create_transport(service_uuid).await?;

        let local_keypair = KeyPair::generate();

        Ok(PairingSession {
            device,
            connection,
            transport,
            local_keypair,
            remote_public_key: None,
            fingerprint: None,
            pin: None,
        })
    }

    pub async fn exchange_keys(&mut self) -> Result<()> {
        // Send local public key
        let public_key_bytes = self.local_keypair.public_key.as_bytes();
        self.transport.send(public_key_bytes).await?;

        // Receive remote public key
        let remote_key_bytes = self.transport.receive().await?;
        let remote_public_key = x25519_dalek::PublicKey::from(<[u8; 32]>::try_from(&remote_key_bytes[..32]).map_err(|_| crate::BluetoothError("Invalid key length".to_string()))?);

        self.remote_public_key = Some(remote_public_key);

        // Generate fingerprint
        let fingerprint = generate_fingerprint(&remote_public_key);
        self.fingerprint = Some(fingerprint.clone());

        // Generate PIN
        let pin = generate_pin_from_fingerprint(&fingerprint);
        self.pin = Some(pin);

        Ok(())
    }

    pub fn get_pin(&self) -> Option<&str> {
        self.pin.as_deref()
    }

    pub fn get_fingerprint(&self) -> Option<&str> {
        self.fingerprint.as_deref()
    }

    pub async fn verify_pin(&self, user_pin: &str) -> Result<bool> {
        if let Some(pin) = &self.pin {
            Ok(pin == user_pin)
        } else {
            Ok(false)
        }
    }

    pub async fn complete_pairing(&self) -> Result<()> {
        // Send confirmation
        self.transport.send(b"PAIRING_COMPLETE").await?;
        // Receive confirmation
        let response = self.transport.receive().await?;
        if &response == b"PAIRING_COMPLETE" {
            Ok(())
        } else {
            Err(crate::BluetoothError("Pairing confirmation failed".to_string()))
        }
    }

    pub fn get_device_info(&self) -> (String, String, String) {
        (
            self.device.id(),
            self.device.name().unwrap_or_default(),
            self.device.address(),
        )
    }

    pub fn get_fingerprint_for_storage(&self) -> Option<String> {
        self.fingerprint.clone()
    }
}