cmake_minimum_required(VERSION 3.27)
project(BlueBeamNative C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add source directories
include_directories(src)
include_directories(src/bluetooth)
include_directories(src/crypto)
include_directories(src/database)
include_directories(src/ui)
include_directories(src/file_transfer)
include_directories(src/event_loop)
include_directories(src/notifications)

# Define the core C backend as a static library
add_library(BlueBeamNative_Core STATIC
    src/bluetooth/bluetooth_manager.c
    src/bluetooth/bluetooth_windows.c
    src/bluetooth/bluetooth_linux.c
    src/crypto/crypto_manager.c
    src/database/db_manager.c
    src/ui/ui_manager.cpp
    src/file_transfer/file_transfer.c
    src/event_loop/event_loop.c
    src/notifications/notification_manager.m
)

find_package(libsodium REQUIRED)
target_link_libraries(BlueBeamNative_Core PRIVATE libsodium::libsodium)

find_package(SQLite3 REQUIRED)
target_link_libraries(BlueBeamNative_Core PRIVATE SQLite3::SQLite3)

if (__linux__)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLIB REQUIRED glib-2.0)
    include_directories(${GLIB_INCLUDE_DIRS})
    target_link_libraries(BlueBeamNative_Core PRIVATE ${GLIB_LIBRARIES})

    pkg_check_modules(LIBNOTIFY REQUIRED libnotify)
    include_directories(${LIBNOTIFY_INCLUDE_DIRS})
    target_link_libraries(BlueBeamNative_Core PRIVATE ${LIBNOTIFY_LIBRARIES})

    pkg_check_modules(SYSTEMD REQUIRED libsystemd)
    include_directories(${SYSTEMD_INCLUDE_DIRS})
    target_link_libraries(BlueBeamNative_Core PRIVATE ${SYSTEMD_LIBRARIES})
endif()

if (_WIN32)
    # Link against Windows Bluetooth APIs
    target_link_libraries(BlueBeamNative_Core PRIVATE Bthprops.lib BluetoothApis.lib)
endif()

if (APPLE)
    # Link the manually compiled Objective-C static library to the core library
    target_link_libraries(BlueBeamNative_Core /Volumes/RCA/beam/BlueBeamNative/lib/libbluetooth_macos.a)
    # Link CoreBluetooth and Foundation frameworks to the core library
    find_library(COREBLUETOOTH CoreBluetooth REQUIRED)
    find_library(FOUNDATION Foundation REQUIRED)
    target_link_libraries(BlueBeamNative_Core ${COREBLUETOOTH} ${FOUNDATION})
endif()

# Find Qt
find_package(Qt6 COMPONENTS Widgets REQUIRED)

# Add the GUI executable
add_executable(BlueBeamNative_GUI
    src/main.cpp # This will be our new GUI entry point
)

# Link Qt libraries and the core C backend library
target_link_libraries(BlueBeamNative_GUI
    Qt6::Widgets
    BlueBeamNative_Core
)

