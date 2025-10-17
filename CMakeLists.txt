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

# Rust integration
include(ExternalProject)
ExternalProject_Add(
    rust_crypto
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/core/crypto
    CONFIGURE_COMMAND ""
    BUILD_COMMAND cargo build --release
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE 1
)
ExternalProject_Add(
    rust_bluetooth
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/core/bluetooth
    CONFIGURE_COMMAND ""
    BUILD_COMMAND cargo build --release
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE 1
)

# SQLite amalgamation
add_library(sqlite3 STATIC src/c/sqlite3.c)
target_include_directories(sqlite3 PUBLIC src/c)

# Core libraries
add_library(database db/database.cpp)
target_link_libraries(database sqlite3 Threads::Threads)

add_library(crypto crypto/crypto.cpp)
add_dependencies(crypto rust_crypto)
target_link_libraries(crypto ${CMAKE_SOURCE_DIR}/core/crypto/target/release/libbluebeam_crypto.a)

# Platform-specific Bluetooth sources
if(APPLE)
    add_library(bluetooth platforms/macos/bluetooth.mm)
elseif(WIN32)
    add_library(bluetooth platforms/windows/bluetooth_win.cpp)
elseif(UNIX AND NOT APPLE)
    add_library(bluetooth platforms/linux/bluetooth_linux.cpp)
endif()
add_dependencies(bluetooth rust_bluetooth)
target_link_libraries(bluetooth ${CMAKE_SOURCE_DIR}/core/bluetooth/target/release/libbluebeam_bluetooth.a)

add_library(settings ui/settings.cpp)

add_library(auto_update ui/auto_update.cpp)
target_link_libraries(auto_update curl)

add_library(messaging ui/messaging.cpp)
target_link_libraries(messaging bluetooth crypto)

add_library(file_transfer ui/file_transfer.cpp)
target_link_libraries(file_transfer bluetooth crypto)

# Platform-specific UI
if(APPLE)
    find_library(COCOA Cocoa REQUIRED)
    find_library(COREBLUETOOTH CoreBluetooth REQUIRED)
    find_library(SECURITY Security REQUIRED)
    find_package(OpenSSL REQUIRED)
    add_executable(BlueBeam ui/main.cpp platforms/macos/ui_macos.mm platforms/macos/ui_macos.h)
    target_link_libraries(BlueBeam ${COCOA} ${COREBLUETOOTH} ${SECURITY} ${OPENSSL_CRYPTO_LIBRARY} database crypto bluetooth messaging file_transfer settings auto_update)
elseif(WIN32)
    add_executable(BlueBeam ui/main.cpp platforms/windows/ui_windows.cpp platforms/windows/ui_windows.h)
    target_link_libraries(BlueBeam database crypto bluetooth messaging file_transfer)
elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK4 REQUIRED gtk4)
    add_executable(BlueBeam ui/main.cpp ui/linux/ui_linux.cpp)
    target_include_directories(BlueBeam PRIVATE ${GTK4_INCLUDE_DIRS})
    target_link_libraries(BlueBeam ${GTK4_LIBRARIES} database crypto bluetooth messaging file_transfer)
endif()

# Install
install(TARGETS BlueBeam DESTINATION bin)
install(DIRECTORY resources/ DESTINATION share/bluebeam FILES_MATCHING PATTERN "*")
install(FILES README.md LICENSE DESTINATION share/doc/bluebeam)

# CPack for installers
set(CPACK_PACKAGE_NAME "BlueBeam")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Offline Bluetooth Chat and File Sharing")
set(CPACK_PACKAGE_VENDOR "BlueBeam Team")

if(APPLE)
    set(CPACK_GENERATOR "ZIP")
    # Code signing disabled for development
    # add_custom_command(TARGET BlueBeam POST_BUILD
    #     COMMAND codesign --sign "Developer ID Application" --deep --force --verbose $<TARGET_FILE:BlueBeam>
    # )
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS")
    # Code signing
    find_program(SIGNTool_EXECUTABLE signtool)
    if(SIGNTool_EXECUTABLE)
        add_custom_command(TARGET BlueBeam POST_BUILD
            COMMAND ${SIGNTool_EXECUTABLE} sign /f cert.pfx /p password $<TARGET_FILE:BlueBeam>
        )
    endif()
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;RPM")
endif()

include(CPack)

