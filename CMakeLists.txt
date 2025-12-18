cmake_minimum_required(VERSION 3.20)
project(BlueBeam VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags per platform
if(APPLE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wextra -march=native")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -march=native")

include_directories(src/cpp)
elseif(WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /O2 /W4 /permissive-")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /W4 /permissive-")
elseif(UNIX AND NOT APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wextra -march=native -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -march=native -fPIC")
endif()

# Find dependencies
find_package(Threads REQUIRED)

# Rust integration with Corrosion
include(FetchContent)
FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.4.7
)
FetchContent_MakeAvailable(Corrosion)

corrosion_import_crate(MANIFEST_PATH rust/core/Cargo.toml)
corrosion_import_crate(MANIFEST_PATH rust/crypto/Cargo.toml)
corrosion_import_crate(MANIFEST_PATH rust/bluetooth/Cargo.toml)
corrosion_import_crate(MANIFEST_PATH rust/database/Cargo.toml)
corrosion_import_crate(MANIFEST_PATH rust/settings/Cargo.toml)

# SQLite amalgamation
add_library(sqlite3 STATIC src/c/sqlite3.c)
target_include_directories(sqlite3 PUBLIC src/c)

# Core libraries
add_library(database src/cpp/database/database.cpp)
target_link_libraries(database sqlite3 Threads::Threads)

add_library(crypto src/cpp/crypto/crypto.cpp)
target_link_libraries(crypto bluebeam_crypto-static)

# Platform-specific Bluetooth sources
if(APPLE)
    add_library(bluetooth src/cpp/bluetooth/bluetooth.mm)
elseif(WIN32)
    add_library(bluetooth src/cpp/bluetooth/bluetooth_win.cpp)
elseif(UNIX AND NOT APPLE)
    add_library(bluetooth src/cpp/bluetooth/bluetooth_linux.cpp)
endif()
target_link_libraries(bluetooth bluebeam_bluetooth-static)

add_library(settings src/cpp/settings/settings.cpp)

add_library(auto_update src/cpp/auto_update/auto_update.cpp)
target_link_libraries(auto_update curl)

add_library(messaging src/cpp/messaging/messaging.cpp)
target_link_libraries(messaging bluetooth crypto)

add_library(file_transfer src/cpp/file_transfer/file_transfer.cpp)
target_link_libraries(file_transfer bluetooth crypto database)

# Platform-specific UI
if(APPLE)
    find_library(COCOA Cocoa REQUIRED)
    find_library(COREBLUETOOTH CoreBluetooth REQUIRED)
    find_library(SECURITY Security REQUIRED)
    find_package(OpenSSL REQUIRED)
    add_executable(BlueBeam src/cpp/main.cpp src/cpp/ui/macos/ui_macos.mm)
    target_link_libraries(BlueBeam ${COCOA} ${COREBLUETOOTH} ${SECURITY} ${OPENSSL_CRYPTO_LIBRARY} bluebeam_core-static database crypto bluetooth messaging file_transfer settings auto_update)
elseif(WIN32)
    add_executable(BlueBeam src/cpp/main.cpp src/cpp/ui/windows/ui_windows.cpp)
    target_link_libraries(BlueBeam bluebeam_core-static database crypto bluetooth messaging file_transfer)
elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK4 REQUIRED gtk4)
    add_executable(BlueBeam src/cpp/main.cpp src/cpp/ui/linux/ui_linux.cpp)
    target_include_directories(BlueBeam PRIVATE ${GTK4_INCLUDE_DIRS})
    target_link_libraries(BlueBeam bluebeam_core-static ${GTK4_LIBRARIES} database crypto bluetooth messaging file_transfer)
endif()

# Install
install(TARGETS BlueBeam DESTINATION bin)
install(DIRECTORY ../resources/ DESTINATION share/bluebeam FILES_MATCHING PATTERN "*")
install(FILES ../README.md ../LICENSE DESTINATION share/doc/bluebeam)

# CPack for installers
set(CPACK_PACKAGE_NAME "BlueBeam")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Offline Bluetooth Chat and File Sharing")
set(CPACK_PACKAGE_VENDOR "BlueBeam Team")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    # Code signing and notarization
    find_program(CODESIGN_EXECUTABLE codesign)
    find_program(ALTOOL_EXECUTABLE altool)
    if(CODESIGN_EXECUTABLE)
        add_custom_command(TARGET BlueBeam POST_BUILD
            COMMAND ${CODESIGN_EXECUTABLE} --sign "Developer ID Application" --deep --force --verbose --options runtime $<TARGET_FILE:BlueBeam>
        )
    endif()
    # Notarization would be done after packaging, e.g., via script
elseif(WIN32)
    set(CPACK_GENERATOR "WIX")
    # Code signing
    find_program(SIGNTool_EXECUTABLE signtool)
    if(SIGNTool_EXECUTABLE)
        add_custom_command(TARGET BlueBeam POST_BUILD
            COMMAND ${SIGNTool_EXECUTABLE} sign /f cert.pfx /p password $<TARGET_FILE:BlueBeam>
        )
    endif()
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;RPM")
    # For AppImage, add custom target
    find_program(LINUXDEPLOY_EXECUTABLE linuxdeploy)
    find_program(APPIMAGETOOL_EXECUTABLE appimagetool)
    if(LINUXDEPLOY_EXECUTABLE AND APPIMAGETOOL_EXECUTABLE)
        add_custom_target(appimage
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target install -- DESTDIR=${CMAKE_BINARY_DIR}/AppDir/usr
            COMMAND ${LINUXDEPLOY_EXECUTABLE} --appdir=${CMAKE_BINARY_DIR}/AppDir --executable=${CMAKE_BINARY_DIR}/AppDir/usr/bin/BlueBeam --desktop-file=${CMAKE_SOURCE_DIR}/resources/bluebeam.desktop --icon-file=${CMAKE_SOURCE_DIR}/resources/icons/icon.png
            COMMAND ${APPIMAGETOOL_EXECUTABLE} ${CMAKE_BINARY_DIR}/AppDir
            COMMENT "Creating AppImage"
        )
    endif()
endif()

include(CPack)

