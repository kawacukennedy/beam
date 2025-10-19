#pragma once
#include <string>

enum class ErrorCode {
    BT001 = 1, // Bluetooth adapter not present
    BT002 = 2, // Permission denied by OS
    BT003 = 3, // Pairing timeout
    NET001 = 4, // Channel send failure
    DB001 = 5, // Database write/read error
    FS001 = 6, // Insufficient disk space
    SEC001 = 7  // Encryption handshake failed
};

struct Error {
    ErrorCode code;
    std::string message;
    std::string user_action;
};

Error get_error(ErrorCode code);