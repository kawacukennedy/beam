#ifndef BLUETOOTH_WINDOWS_H
#define BLUETOOTH_WINDOWS_H

#include "bluetooth_manager.h"

// Windows specific Bluetooth manager implementation
IBluetoothManager* get_windows_bluetooth_manager(void);

#endif // BLUETOOTH_WINDOWS_H