# BlueBeam
 
**Offline. Private. Instant Hardware-Level Bluetooth Communication.**

BlueBeam is a cross-platform offline peer-to-peer chat and file-sharing application for laptops. Written in C, C++, and Rust to allow direct hardware-level Bluetooth communication, ultra-low latency, high efficiency, and maximum security. Provides encrypted messaging and file transfer without internet dependency.

## Table of Contents

- [Features](#features)
- [Tech Stack](#tech-stack)
- [UI Specifications](#ui-specifications)
- [Performance Metrics](#performance-metrics)
- [Security Compliance](#security-compliance)
- [Building and Running](#building-and-running)
- [Documentation](#documentation)
- [License](#license)

## Features

- **Direct Hardware Bluetooth**: Low-level OS APIs for maximum performance and security
- **End-to-End Encryption**: Curve25519 ECDH handshake, AES-256 GCM sessions, RSA-4096 identity
- **Native UI**: Platform-optimized interfaces (Cocoa/macOS, Win32/Windows, GTK4/Linux)
- **Offline Operation**: Zero internet dependency, local-only encrypted storage
- **High Performance**: <100ms message latency, >=5 MB/s file throughput, <400MB RAM
- **Cross-Platform**: macOS 14+, Windows 10+, Linux (Ubuntu 22.04+, Fedora 39+, Arch)

## Tech Stack

- **Languages**: C17, C++20, Rust 1.80
- **UI Frameworks**:
  - macOS: Cocoa + CoreGraphics
  - Windows: Win32 API + Direct2D/GDI+
  - Linux: GTK4/Qt6 + OpenGL
- **Database**: SQLite compiled from source with UUID primary keys and indexes
- **Bluetooth Stack**: Direct OS APIs with platform-specific threading and timeouts
- **Messaging Protocol**: RFCOMM SPP with binary-packed structs + CRC32
- **File Transfer**: OBEX over RFCOMM with 128KB chunks, SHA-256 integrity, resume support
- **Encryption**: Perfect forward secrecy with session key rotation and zeroization
- **Build System**: CMake with platform-specific compiler optimizations
- **Packaging**: CPack for native installers with auto-update support

## UI Specifications

### Windows
- **Onboarding**: Splash screen fade, Bluetooth permission dialog, profile setup with avatar upload, tutorial overlay
- **Device Discovery**: Responsive grid (2-6 columns), device cards with signal/trust icons, manual refresh, filter search
- **Chat Window**: Sidebar (250px) + flexible main panel, message bubbles with corner radius 12px, search/emoji/attachment features
- **File Transfer Modal**: File preview, progress bar with ETA/speed, pause/resume/cancel controls
- **Settings**: Profile/Preferences/Storage/Security/About sections with theme toggle, download path, trust management

### UX Flows
- **First Time**: Launch → Splash → Permissions → Profile → Tutorial → Device Discovery
- **Pairing**: Select device → PIN prompt → Confirm → Store trusted → Success toast
- **Messaging**: Open chat → Type → Encrypt → Send → ACK → Display bubble → Update receipt
- **File Transfer**: Drag file → Preview → Confirm → Encrypt & chunk → Transfer → Receiver accepts → Assemble → SHA-256 validate → Success toast
- **Disconnection**: Out-of-range → Reconnect banner → Retry 3x → Fail → Mark offline
- **Error Handling**: Handshake failure → Re-pair prompt, Transfer timeout → Retry, DB error → Fallback to memory cache

### Accessibility
- Keyboard navigation and shortcuts
- Screen reader support with native OS APIs
- High-contrast theme and WCAG AA compliance
- Localization support

## Performance Metrics
- **Message Latency**: <100ms
- **File Throughput**: >=5 MB/s
- **CPU Usage**: <10%
- **RAM Usage**: <400MB
- **Battery Drain**: <3%/hour

## Security Compliance
- **Standards**: OWASP Top 10, AES-NIST, FIPS 140-3
- **Data Handling**: Local-only encrypted storage
- **Audit**: Quarterly static and dynamic scans
- **Key Management**: Platform-specific secure storage (Keychain/macOS, DPAPI/Windows, GNOME Keyring/Linux)
- **Perfect Forward Secrecy**: Session key rotation and zeroization on disconnect

## Bluetooth Stack
- **Scan Interval**: 3000ms
- **Connection Timeouts**: macOS 8000ms, Windows 10000ms, Linux 8000ms
- **Max Connections**: macOS 6, Windows 6, Linux 8
- **Threading**: Platform-native (dispatch queues/macOS, Win32 threads/Windows, POSIX/Linux)
- **Retry Policy**: Max 3 attempts, 500ms backoff

## Messaging Protocol
- **Transport**: RFCOMM SPP
- **Format**: Binary-packed struct + CRC32 checksum
- **Max Size**: 65536 bytes
- **Delivery**: ACK + 3 retries exponential backoff

## File Transfer Protocol
- **Transport**: OBEX over RFCOMM
- **Chunk Size**: 131072 bytes
- **Max File Size**: 4294967296 bytes
- **Integrity**: SHA-256 per chunk + full file
- **Resume Support**: Checkpoint by offset
- **Queue**: FIFO with small file priority

## Building and Running

### Prerequisites
- **macOS**: Xcode 15+, CMake 3.20+, Rust 1.80
- **Windows**: Visual Studio 2022+, CMake 3.20+, Rust 1.80
- **Linux**: GCC 11+, CMake 3.20+, Rust 1.80, GTK4/Qt6 development packages

### Build Steps
1. **Clone and navigate**:
   ```bash
   git clone https://github.com/kawacukennedy/beam.git
   cd beam/BlueBeamNative
   ```

2. **Configure with CMake**:
   ```bash
   mkdir build && cd build
   cmake ..
   ```

3. **Build**:
   ```bash
   cmake --build . --config Release
   ```

4. **Run**:
   ```bash
   ./BlueBeam
   ```

### Create Installers
```bash
cpack
```
Generates:
- macOS: `BlueBeam-1.0.0.dmg`
- Windows: `BlueBeam-1.0.0.exe`
- Linux: `BlueBeam-1.0.0.AppImage`, `.deb`, `.rpm`

## Documentation

- [User Manual](docs/USER_MANUAL.md) - Installation, setup, and usage guide
- [Troubleshooting Guide](docs/TROUBLESHOOTING.md) - Common issues and solutions
- Inline code documentation available in source files

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
