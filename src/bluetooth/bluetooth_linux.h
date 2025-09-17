#ifndef BLUETOOTH_LINUX_H
#define BLUETOOTH_LINUX_H

#include "bluetooth_manager.h"

// Linux specific Bluetooth manager implementation
IBluetoothManager* get_linux_bluetooth_manager(void);

#endif // BLUETOOTH_LINUX_H
