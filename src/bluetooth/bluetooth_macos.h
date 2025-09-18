#ifndef BLUETOOTH_MACOS_H
#define BLUETOOTH_MACOS_H

#include "bluetooth_manager.h"
#include <sodium.h>

// Structure to manage ongoing file transfers
typedef struct {
    FILE *file_handle;
    char file_path[1024];
    char file_name[256]; // To store just the filename from metadata
    long file_size;
    long bytes_transferred;
    unsigned long long current_chunk_index;
    unsigned char encryption_key[crypto_secretbox_KEYBYTES]; // Key for this specific transfer
    bool is_sending; // true for sending, false for receiving
} FileTransferState;

// macOS specific Bluetooth manager implementation
IBluetoothManager* get_macos_bluetooth_manager(void);

#endif // BLUETOOTH_MACOS_H