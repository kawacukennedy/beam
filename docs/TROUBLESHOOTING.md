# BlueBeam Troubleshooting Guide

This guide addresses common issues with BlueBeam. If you encounter a problem not listed here, check the logs or contact support.

## Table of Contents

- [Bluetooth Issues](#bluetooth-issues)
- [Pairing Problems](#pairing-problems)
- [Connection Failures](#connection-failures)
- [Messaging Issues](#messaging-issues)
- [File Transfer Problems](#file-transfer-problems)
- [Performance Issues](#performance-issues)
- [Platform-Specific Issues](#platform-specific-issues)
- [Security Concerns](#security-concerns)

## Bluetooth Issues

### Bluetooth Not Detected
- Ensure Bluetooth is enabled in your system settings.
- Restart BlueBeam.
- Check if Bluetooth hardware is functioning (try pairing with another device).

### Discovery Fails
- Move devices closer (within 10m range).
- Ensure both devices have Bluetooth enabled and discoverable.
- Restart Bluetooth service:
  - macOS: System Settings > Bluetooth > Turn off/on
  - Windows: Settings > Devices > Bluetooth > Turn off/on
  - Linux: `sudo systemctl restart bluetooth`

## Pairing Problems

### PIN Mismatch
- Ensure the PIN displayed on both devices matches exactly.
- Retry pairing; PINs are generated from device fingerprints.

### Pairing Timeout
- Check Bluetooth signal strength.
- Close other Bluetooth connections.
- Restart both devices.

### Device Not Found
- Confirm the target device is running BlueBeam and in discovery mode.
- Refresh the device list manually.

## Connection Failures

### Connection Drops
- Stay within Bluetooth range.
- Avoid interference from Wi-Fi, microwaves, or other devices.
- Reconnect manually; app retries up to 3 times with backoff.

### Handshake Failure
- Re-pair the devices.
- Check for software updates.
- Verify device compatibility (supported platforms only).

## Messaging Issues

### Messages Not Sending
- Confirm device is connected and paired.
- Check message size (max 64KB).
- Restart the app.

### Messages Not Received
- Ensure receiver app is open and not in sleep mode.
- Check delivery receipts; resend if needed.

### Message History Missing
- Data is stored locally; check storage settings.
- If corrupted, clear cache (may lose data).

## File Transfer Problems

### Transfer Fails
- Check file size (max 4GB).
- Ensure sufficient storage space.
- Verify file integrity post-transfer.

### Slow Transfer Speed
- Move devices closer.
- Close other apps using Bluetooth.
- Expected speed: >=5 MB/s under optimal conditions.

### Transfer Interrupted
- Resume supported; check progress and retry.
- If resume fails, restart transfer.

### File Corruption
- SHA-256 verification fails; retry transfer.
- Check local storage for corruption.

## Performance Issues

### High CPU/RAM Usage
- Close other applications.
- Restart BlueBeam.
- Check for background processes.

### Battery Drain
- Bluetooth usage may drain battery; connect to power if needed.
- Expected drain: <3%/hour.

### Latency Issues
- Ensure devices are in range.
- Restart connection.

## Platform-Specific Issues

### macOS
- Grant Bluetooth permissions in System Preferences > Security & Privacy.
- If using Xcode, ensure developer tools are installed.

### Windows
- Run as administrator if permission issues.
- Update Bluetooth drivers.

### Linux
- Install Bluetooth packages: `sudo apt install bluez` (Ubuntu).
- Ensure GTK4/Qt6 is installed for UI.

## Security Concerns

### Suspicious Activity
- Verify device fingerprints.
- Unpair unknown devices.
- Check logs for anomalies.

### Key Management
- Keys are managed automatically; do not manually edit.
- If compromised, reset app data.

### Encryption Errors
- Re-pair devices.
- Update to latest version.

If issues persist, collect logs and report to the project repository.