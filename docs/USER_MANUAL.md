# BlueBeam User Manual

BlueBeam is an offline, private peer-to-peer chat and file-sharing application that uses Bluetooth for direct hardware-level communication. This manual guides you through installation, setup, and usage.

## Table of Contents

- [Installation](#installation)
- [First-Time Setup](#first-time-setup)
- [Pairing Devices](#pairing-devices)
- [Messaging](#messaging)
- [File Sharing](#file-sharing)
- [Security Features](#security-features)
- [Settings](#settings)

## Installation

BlueBeam supports macOS 14+, Windows 10+, and Linux (Ubuntu 22.04+, Fedora 39+, Arch).

### Prerequisites

- **macOS**: Xcode 15+, CMake 3.20+, Rust 1.80
- **Windows**: Visual Studio 2022+, CMake 3.20+, Rust 1.80
- **Linux**: GCC 11+, CMake 3.20+, Rust 1.80, GTK4/Qt6 development packages

### Build from Source

1. Clone the repository:
   ```bash
   git clone https://github.com/kawacukennedy/beam.git
   cd beam
   ```

2. Configure with CMake:
   ```bash
   mkdir build && cd build
   cmake ..
   ```

3. Build:
   ```bash
   cmake --build . --config Release
   ```

4. Run:
   ```bash
   ./BlueBeam
   ```

### Installers

Generate native installers using CPack:
```bash
cpack
```

This creates:
- macOS: `BlueBeam-1.0.0.dmg`
- Windows: `BlueBeam-1.0.0.exe`
- Linux: `BlueBeam-1.0.0.AppImage`, `.deb`, `.rpm`

## First-Time Setup

1. Launch BlueBeam.
2. Grant Bluetooth permissions when prompted.
3. Set up your profile: Enter a name and optionally upload an avatar.
4. Complete the tutorial overlay to familiarize yourself with the interface.

## Pairing Devices

Pairing establishes a secure connection with another BlueBeam device.

1. Open BlueBeam and navigate to the Device Discovery page.
2. The app scans for nearby Bluetooth devices running BlueBeam.
3. Select a device from the list.
4. A PIN prompt appears on both devices.
5. Enter the PIN on the initiating device to confirm pairing.
6. Once paired, the device is marked as trusted and appears in your device list.

Paired devices can communicate securely without re-pairing unless manually unpaired.

## Messaging

Send encrypted messages to paired devices.

1. Select a paired device from the sidebar.
2. Type your message in the input field.
3. Press Enter or click Send.
4. Messages appear as bubbles in the chat window.
5. Delivery receipts update when the message is acknowledged.

Features:
- Search messages
- Emoji support
- Message history stored locally and encrypted

## File Sharing

Share files securely over Bluetooth.

1. In a chat with a paired device, click the attachment button.
2. Select a file to share (max 4GB).
3. Preview the file and confirm transfer.
4. The file is encrypted, chunked, and sent.
5. The receiver sees a transfer notification and can accept or decline.
6. Progress is shown with ETA and speed.
7. Transfer can be paused, resumed, or canceled.
8. Upon completion, integrity is verified with SHA-256.

Features:
- Resume interrupted transfers
- Queue multiple files (small files prioritized)
- Local encrypted storage

## Security Features

BlueBeam prioritizes privacy and security:

- **End-to-End Encryption**: Messages and files use Curve25519 ECDH for key exchange, AES-256 GCM for sessions, RSA-4096 for identity.
- **Perfect Forward Secrecy**: Session keys rotate and are zeroized on disconnect.
- **Local Storage**: All data stored locally, encrypted, with no internet dependency.
- **Key Management**: Keys stored in platform-specific secure storage (Keychain/macOS, DPAPI/Windows, GNOME Keyring/Linux).
- **Offline Operation**: No data sent over internet; all communication via Bluetooth.

Verify security by checking device fingerprints during pairing.

## Settings

Access settings from the menu:

- **Profile**: Update name, avatar, status.
- **Preferences**: Theme toggle, download path.
- **Storage**: Manage local data, clear history.
- **Security**: View trusted devices, manage keys, security audit.
- **About**: Version info, license.