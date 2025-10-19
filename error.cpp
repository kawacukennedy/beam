#include "error.h"

Error get_error(ErrorCode code) {
    switch (code) {
        case ErrorCode::BT001:
            return {code, "Bluetooth adapter not present", "Enable Bluetooth or use a device with Bluetooth adapter"};
        case ErrorCode::BT002:
            return {code, "Bluetooth permission denied", "Grant Bluetooth permission in OS settings"};
        case ErrorCode::BT003:
            return {code, "Pairing timed out", "Retry pairing and keep devices close"};
        case ErrorCode::NET001:
            return {code, "Channel send failure", "Check Bluetooth connection and retry"};
        case ErrorCode::DB001:
            return {code, "Database write failed", "Restart app; if persists export logs and contact support"};
        case ErrorCode::FS001:
            return {code, "Insufficient disk space", "Free up disk space or change download path"};
        case ErrorCode::SEC001:
            return {code, "Encryption handshake failed", "Re-pair device; check device clocks and retry"};
        default:
            return {code, "Unknown error", "Contact support"};
    }
}