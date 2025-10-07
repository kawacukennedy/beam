cmake_minimum_required(VERSION 3.28)
project(BlueLinkManager CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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
    src/bluetooth/file_transfer_worker.cpp
    src/crypto/crypto_manager.c
    src/database/db_manager.c
    src/ui/ui_manager.cpp
    src/file_transfer/file_transfer.c
    src/event_loop/event_loop.c
    src/utils/json_file_logger.cpp
)

if(APPLE)
    list(APPEND CORE_SOURCES src/bluetooth/bluetooth_macos.mm src/notifications/notification_manager.m)
elseif(WIN32)
    list(APPEND CORE_SOURCES src/bluetooth/bluetooth_windows.c)
elseif(UNIX AND NOT APPLE)
    list(APPEND CORE_SOURCES src/bluetooth/bluetooth_linux.c)
endif()

add_library(BlueLinkManager_Core STATIC ${CORE_SOURCES})

# Collect all libraries for BlueLinkManager_Core
set(BLUELINK_CORE_LIBRARIES)

# libsodium
find_library(LIBSODIUM_LIBRARY NAMES sodium PATHS /usr/local/lib /opt/homebrew/lib REQUIRED)
list(APPEND BLUELINK_CORE_LIBRARIES ${LIBSODIUM_LIBRARY})

# SQLite3
find_package(SQLite3 REQUIRED)
list(APPEND BLUELINK_CORE_LIBRARIES ${SQLITE3_LIBRARIES})

# Linux specific libraries
if (__linux__)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLIB REQUIRED glib-2.0)
    list(APPEND BLUELINK_CORE_LIBRARIES ${GLIB_LIBRARIES})

    pkg_check_modules(LIBNOTIFY REQUIRED libnotify)
    list(APPEND BLUELINK_CORE_LIBRARIES ${LIBNOTIFY_LIBRARIES})

    pkg_check_modules(SYSTEMD REQUIRED libsystemd)
    list(APPEND BLUELINK_CORE_LIBRARIES ${SYSTEMD_LIBRARIES})

    # For json-c
    pkg_check_modules(JSONC REQUIRED json-c)
    list(APPEND BLUELINK_CORE_LIBRARIES ${JSONC_LIBRARIES})
endif()

# Windows specific libraries
if (_WIN32)
    list(APPEND BLUELINK_CORE_LIBRARIES Bthprops.lib BluetoothApis.lib Setupapi.lib)
    # For json-c, assuming it's available via vcpkg or similar
    # find_package(json-c CONFIG REQUIRED) # If json-c provides a config file
    # list(APPEND BLUELINK_CORE_LIBRARIES json-c::json-c) # Or similar
    # For now, assuming json-c is linked manually or not used in Windows Bluetooth
endif()

# macOS specific frameworks
if (APPLE)
    find_library(COREBLUETOOTH CoreBluetooth REQUIRED)
    find_library(FOUNDATION Foundation REQUIRED)
    find_library(USERNOTIFICATIONS UserNotifications REQUIRED)
    list(APPEND BLUELINK_CORE_LIBRARIES ${COREBLUETOOTH} ${FOUNDATION} ${USERNOTIFICATIONS})
    # Link the manually compiled Objective-C static library to the core library
    # This path needs to be absolute or relative to the build directory
    # Assuming it's in the project root's lib directory
    list(APPEND BLUELINK_CORE_LIBRARIES ${CMAKE_SOURCE_DIR}/lib/libbluetooth_macos.a)
endif()

# Link all collected libraries to BlueLinkManager_Core
target_link_libraries(BlueLinkManager_Core PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Qml # Added for ui_manager.cpp to find QML headers
    Qt6::Quick # Added for ui_manager.cpp to find QML headers
    ${BLUELINK_CORE_LIBRARIES}
)

# Find Qt
find_package(Qt6 COMPONENTS Widgets Qml Quick REQUIRED)

# Add the GUI executable
add_executable(BlueLinkManager_GUI
    src/main.cpp
    src/bluelink_app.cpp
)

# Add QML module
qt_add_qml_module(BlueLinkManager_GUI
    URI com.bluelinkmanager
    VERSION 1.0
    QML_FILES
        src/qml/main.qml
        src/qml/Theme.qml
        src/qml/SideBar.qml
        src/qml/TopBar.qml
        src/qml/DashboardScreen.qml
        src/qml/AdapterCard.qml
        src/qml/RecentDevicesCard.qml
        src/qml/DeviceListScreen.qml
        src/qml/DeviceCard.qml
        src/qml/ChatScreen.qml
        src/qml/MessageBubble.qml
        src/qml/InputField.qml
        src/qml/SendButton.qml
        src/qml/FileTransferScreen.qml
        src/qml/SettingsScreen.qml
        src/qml/GeneralSettings.qml
        src/qml/BluetoothSettings.qml
        src/qml/AdvancedSettings.qml
        src/qml/PrimaryButton.qml
        src/qml/Toast.qml
        src/qml/TypingIndicator.qml
)

# Link Qt libraries and the core C backend library
target_link_libraries(BlueLinkManager_GUI PRIVATE
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    BlueLinkManager_Core
    SQLite::SQLite3
)

# ----------------------------------------------------------------------------
# CPack Installer Configuration
# ----------------------------------------------------------------------------

# -- General Package Information --
set(CPACK_PACKAGE_NAME "BlueConnect Desktop")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VENDOR "YourCompany Inc.")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BlueConnect Desktop - Connect & Share Instantly")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://yourcompany.com/blueconnect")
set(CPACK_PACKAGE_CONTACT "support@yourcompany.com")

# -- License and Resources --
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.png")

# -- Install Rules --
# This tells CPack what to install. The main executable target is 'BlueLinkManager_GUI'.
install(TARGETS BlueLinkManager_GUI
    BUNDLE DESTINATION .  # For macOS .app bundle
    RUNTIME DESTINATION bin # For Windows/Linux executables
    LIBRARY DESTINATION lib # For shared libraries
    ARCHIVE DESTINATION lib # For static libraries
)

# Install resource files
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources/"
    DESTINATION share/resources
)


# -- macOS Specific Settings (DragNDrop DMG Generator) --
if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    # Add a symlink to the /Applications folder to the DMG for easy installation
    set(CPACK_DMG_FILESYSTEM "APFS")
    set(CPACK_DMG_FORMAT "UDBZ")
    set(CPACK_DMG_APPLICATION_BUNDLE_FIRST "BlueLinkManager_GUI.app")
endif()

# -- Windows Specific Settings (NSIS Generator) --
if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "BlueConnect Desktop")
    set(CPACK_NSIS_INSTALLER_ICON_CODE "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.ico")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.ico")
    # Create a start menu shortcut
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '\\$SMPROGRAMS\\BlueConnect Desktop.lnk' '\\$INSTDIR\\bin\\BlueLinkManager_GUI.exe'")
endif()

# -- Linux Specific Settings (DEB/RPM Generators) --
if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "support@yourcompany.com")
    set(CPACK_RPM_PACKAGE_LICENSE "Proprietary")
endif()


# -- Include the CPack module to enable packaging --
include(CPack)

