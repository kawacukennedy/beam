cmake_minimum_required(VERSION 3.27)
project(BlueBeamNative C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable AUTOMOC and AUTORCC for Qt projects
# CMAKE_AUTOUIC is disabled as we are moving to QML and no .ui files are used.
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

# Add source directories
include_directories(src)
include_directories(src/bluetooth)
include_directories(src/crypto)
include_directories(src/database)
include_directories(src/ui)
include_directories(src/file_transfer)
include_directories(src/event_loop)
include_directories(src/notifications)
include_directories(/usr/local/include)
include_directories(/opt/homebrew/include)

# Define the core C backend as a static library
set(CORE_SOURCES
    src/bluetooth/bluetooth_manager.c
    src/crypto/crypto_manager.c
    src/database/db_manager.c
    src/ui/ui_manager.cpp
    src/file_transfer/file_transfer.c
    src/event_loop/event_loop.c
)

if(APPLE)
    list(APPEND CORE_SOURCES src/bluetooth/bluetooth_macos.m src/notifications/notification_manager.m)
elseif(WIN32)
    list(APPEND CORE_SOURCES src/bluetooth/bluetooth_windows.c)
elseif(UNIX AND NOT APPLE)
    list(APPEND CORE_SOURCES src/bluetooth/bluetooth_linux.c)
endif()

add_library(BlueBeamNative_Core STATIC ${CORE_SOURCES})

# Collect all libraries for BlueBeamNative_Core
set(BLUEBEAM_CORE_LIBRARIES)

# libsodium
find_library(LIBSODIUM_LIBRARY NAMES sodium PATHS /usr/local/lib /opt/homebrew/lib REQUIRED)
list(APPEND BLUEBEAM_CORE_LIBRARIES ${LIBSODIUM_LIBRARY})

# SQLite3
find_package(SQLite3 REQUIRED)
list(APPEND BLUEBEAM_CORE_LIBRARIES ${SQLITE3_LIBRARIES})

# Linux specific libraries
if (__linux__)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLIB REQUIRED glib-2.0)
    list(APPEND BLUEBEAM_CORE_LIBRARIES ${GLIB_LIBRARIES})

    pkg_check_modules(LIBNOTIFY REQUIRED libnotify)
    list(APPEND BLUEBEAM_CORE_LIBRARIES ${LIBNOTIFY_LIBRARIES})

    pkg_check_modules(SYSTEMD REQUIRED libsystemd)
    list(APPEND BLUEBEAM_CORE_LIBRARIES ${SYSTEMD_LIBRARIES})

    # For json-c
    pkg_check_modules(JSONC REQUIRED json-c)
    list(APPEND BLUEBEAM_CORE_LIBRARIES ${JSONC_LIBRARIES})
endif()

# Windows specific libraries
if (_WIN32)
    list(APPEND BLUEBEAM_CORE_LIBRARIES Bthprops.lib BluetoothApis.lib Setupapi.lib)
    # For json-c, assuming it's available via vcpkg or similar
    # find_package(json-c CONFIG REQUIRED) # If json-c provides a config file
    # list(APPEND BLUEBEAM_CORE_LIBRARIES json-c::json-c) # Or similar
    # For now, assuming json-c is linked manually or not used in Windows Bluetooth
endif()

# macOS specific frameworks
if (APPLE)
    find_library(COREBLUETOOTH CoreBluetooth REQUIRED)
    find_library(FOUNDATION Foundation REQUIRED)
    list(APPEND BLUEBEAM_CORE_LIBRARIES ${COREBLUETOOTH} ${FOUNDATION})
    # Link the manually compiled Objective-C static library to the core library
    # This path needs to be absolute or relative to the build directory
    # Assuming it's in the project root's lib directory
    list(APPEND BLUEBEAM_CORE_LIBRARIES ${CMAKE_SOURCE_DIR}/lib/libbluetooth_macos.a)
endif()

# Link all collected libraries to BlueBeamNative_Core
target_link_libraries(BlueBeamNative_Core PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Qml # Added for ui_manager.cpp to find QML headers
    Qt6::Quick # Added for ui_manager.cpp to find QML headers
    ${BLUEBEAM_CORE_LIBRARIES}
)

# Find Qt
find_package(Qt6 COMPONENTS Widgets Qml Quick REQUIRED)

# Add the GUI executable
add_executable(BlueBeamNative_GUI
    src/main.cpp
    src/bluelink_app.cpp
)

# Generate resource file using rcc
set(QML_RESOURCE_FILE_CPP ${CMAKE_CURRENT_BINARY_DIR}/qrc_qml.cpp)
add_custom_command(
    OUTPUT ${QML_RESOURCE_FILE_CPP}
    COMMAND /usr/local/Cellar/qt/6.9.2/share/qt/libexec/rcc -name qml_resources -o ${QML_RESOURCE_FILE_CPP} -root ${CMAKE_CURRENT_SOURCE_DIR}/src/qml ${CMAKE_CURRENT_SOURCE_DIR}/src/qml/qml.qrc
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/qml/qml.qrc
            ${CMAKE_CURRENT_SOURCE_DIR}/src/qml/main.qml
            ${CMAKE_CURRENT_SOURCE_DIR}/src/qml/Theme.qml
)

# Add the generated resource file to the executable sources
target_sources(BlueBeamNative_GUI PRIVATE ${QML_RESOURCE_FILE_CPP})

# Link Qt libraries and the core C backend library
target_link_libraries(BlueBeamNative_GUI PRIVATE
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    BlueBeamNative_Core
)
