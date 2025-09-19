#ifndef BLUETOOTH_MACOS_H
#define BLUETOOTH_MACOS_H

#include "bluetooth_manager.h"

// macOS specific Bluetooth manager implementation
IBluetoothManager* get_macos_bluetooth_manager(void);

#endif // BLUETOOTH_MACOS_H