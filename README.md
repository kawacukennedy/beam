# BlueBeamNative

BlueBeamNative is a native, low-level cross-platform desktop application designed for secure Bluetooth messaging and file sharing between laptops. Built in C11 for direct OS integration, it leverages native UI components per OS, robust encryption, chunked file transfers, notifications, and optimized user experience flows.

## Features

*   **Secure Bluetooth Communication:** Enables encrypted messaging and file sharing between laptops.
*   **Cross-Platform Native UI:** Utilizes native UI components for macOS (Cocoa + SwiftUI), Windows (WinUI 3), and Linux (GTK 4 + libadwaita) for an optimized user experience.
*   **Robust Encryption:** Employs libsodium for XChaCha20-Poly1305 AEAD message and file encryption, Curve25519 ECDH for key exchange, and SHA-256 for integrity checks.
*   **Chunked File Transfers:** Supports efficient, resumable file transfers with 8MB chunks, encryption, and progress tracking.
*   **Native Notifications:** Integrates with OS-specific notification systems (NSUserNotificationCenter, WinUI Toast, DBus/libnotify).
*   **Local Data Storage:** Uses SQLite3 (with optional SQLCipher for encryption) to store device, message, and file transfer logs.

## Tech Stack

*   **Core Language:** C11
*   **Build System:** CMake 3.27+
*   **Bluetooth:**
    *   **macOS:** CoreBluetooth.framework (Objective-C bridge)
    *   **Windows:** Win32 Bluetooth APIs (for BLE GATT)
    *   **Linux:** BlueZ stack via DBus (sd-bus)
*   **UI Frameworks:**
    *   **macOS:** Qt 6 (mimicking Cocoa/SwiftUI principles)
    *   **Windows:** Qt 6 (mimicking WinUI 3 principles)
    *   **Linux:** Qt 6 (mimicking GTK 4/libadwaita principles)
*   **Database:** SQLite3 (SQLCipher optional)
*   **Cryptography:** libsodium

## Architecture

The application is modular, with distinct components handling specific functionalities:

*   **`bluetooth_manager`:** Handles device discovery, pairing, connection, disconnection, and data transfer (messages and files) across platforms.
*   **`crypto_manager`:** Manages session key generation, encryption/decryption of all data, and integrity verification.
*   **`db_manager`:** Provides a SQLite wrapper for persistent storage of devices, messages, and file transfer metadata.
*   **`ui_manager`:** An abstraction layer for native UI calls and event dispatching, ensuring a consistent interface for the C backend.
*   **`file_transfer`:** (Planned as a separate module, currently integrated within bluetooth_manager for simplicity) Handles chunking, retries, pause/resume, and progress tracking for file transfers.
*   **`event_loop`:** Provides a cross-platform main loop for handling OS-specific events.
*   **`notification_manager`:** Manages native OS notifications.

## UI/UX Specifications

### Design System
*   **Colors:** Primary (`#1E88E5`), Primary Dark (`#1565C0`), Secondary (`#FFB300`), Background Light (`#FFFFFF`), Background Dark (`#121212`), Text Primary (`#212121`), Text Secondary (`#757575`), Success (`#43A047`), Error (`#E53935`), Divider (`#E0E0E0`), Hover (`#E3F2FD`), Selection (`#BBDEFB`).
*   **Typography:** System default font, with defined sizes and weights for H1, H2, Body, and Caption.
*   **Components:** Standardized buttons, input fields, list items, progress bars, and icons with defined dimensions, radii, padding, and states.

### Screens
*   **Splash Screen:** Centered logo, spinner, 2s fade to pairing screen.
*   **Pairing Screen:** Sidebar with device list, connect/refresh buttons, scrollable device list showing name, RSSI, trust icon, last seen. Empty state with illustration and text.
*   **Chat Screen:** Two-column layout (device list left, chat right). Message bubbles (outgoing: primary background, white text, right-aligned; incoming: light grey background, dark text, left-aligned). Timestamps below bubbles, three-dot typing indicator. Full-width, rounded input bar with Send and Attach File buttons.
*   **File Transfer Screen:** Transfer items with file type icon, truncated filename, smooth progress bar, and actions (Pause, Resume, Cancel). Red banner for error states.
*   **Settings Screen:** Options for dark/light theme, trusted device management, storage usage, and about/version info.

## Build and Run Instructions

This guide provides instructions to build and run the BlueBeamNative application on macOS, Windows, and Linux.

### Prerequisites

Before you begin, ensure you have the following installed:

*   **CMake:** Version 3.27 or higher.
*   **Qt 6:** Development libraries for your respective operating system.
*   **libsodium:** Development libraries.
*   **SQLite3:** Development libraries.
*   **Git:** For cloning the repository.

#### Platform-Specific Prerequisites:

*   **macOS:**
    *   Xcode Command Line Tools (includes Clang, Make, etc.).
    *   Qt 6 for macOS (e.g., via Homebrew or Qt installer).
    *   libsodium (e.g., `brew install libsodium`).
    *   SQLite3 (usually pre-installed or `brew install sqlite`).
*   **Windows:**
    *   Visual Studio 2019 or newer (with C++ desktop development workload).
    *   Qt 6 for MSVC (download from Qt website or use `vcpkg`).
    *   libsodium (e.g., via `vcpkg install libsodium:x64-windows`).
    *   SQLite3 (e.g., via `vcpkg install sqlite3:x64-windows`).
*   **Linux:**
    *   GCC/G++ (build-essential package on Debian/Ubuntu).
    *   Qt 6 development libraries (e.g., `sudo apt install qt6-base-dev`).
    *   libsodium (e.g., `sudo apt install libsodium-dev`).
    *   SQLite3 (e.g., `sudo apt install libsqlite3-dev`).
    *   BlueZ development libraries (e.g., `sudo apt install libbluetooth-dev`).
    *   libsystemd development libraries (e.g., `sudo apt install libsystemd-dev`).
    *   libnotify development libraries (e.g., `sudo apt install libnotify-dev`).

### Building the Application

Follow these steps to build the application on your system:

1.  **Clone the Repository:**
    ```bash
    git clone https://github.com/kawacukennedy/beam.git
    cd beam/BlueBeamNative
    ```

2.  **Create a Build Directory:**
    It's good practice to build outside the source directory.
    ```bash
    mkdir build
    cd build
    ```

3.  **Configure CMake:**
    Run CMake to generate the build system files. The command varies slightly by OS.

    *   **macOS (Xcode Project):**
        ```bash
        cmake -G "Xcode" ..
        ```
        This will generate an Xcode project (`BlueBeamNative.xcodeproj`) in the `build` directory.

    *   **Windows (Visual Studio Solution):**
        ```bash
        cmake -G "Visual Studio 17 2022" ..
        ```
        Replace `"Visual Studio 17 2022"` with your installed Visual Studio version if different (e.g., `"Visual Studio 16 2019"`). This will generate a Visual Studio solution (`BlueBeamNative.sln`).

    *   **Linux (Makefiles):**
        ```bash
        cmake ..
        ```
        This will generate `Makefile`s in the `build` directory.

4.  **Build the Project:**
    Use the CMake build command, which is cross-platform.
    ```bash
    cmake --build .
    ```
    This command will compile the source code and create the executable.

### Running the Application

After a successful build, you can run the application:

*   **macOS:**
    Navigate to the `build` directory and run the application bundle.
    ```bash
    ./Debug/BlueBeamNative_GUI.app/Contents/MacOS/BlueBeamNative_GUI
    ```
    (If you built in Release mode, replace `Debug` with `Release`).
    Alternatively, open `BlueBeamNative.xcodeproj` in Xcode and run from there.

*   **Windows:**
    Navigate to the `build` directory (or `build/Debug` or `build/Release` depending on your build type) and run the executable.
    ```bash
    .\Debug\BlueBeamNative_GUI.exe
    ```
    (If you built in Release mode, replace `Debug` with `Release`).
    Alternatively, open `BlueBeamNative.sln` in Visual Studio and run from there.

*   **Linux:**
    Navigate to the `build` directory and run the executable.
    ```bash
    ./BlueBeamNative_GUI
    ```

### Important Notes:

*   **Bluetooth Permissions:** On macOS and Linux, you might need to grant Bluetooth permissions to the application or ensure your user has the necessary privileges to access Bluetooth devices (e.g., being part of the `bluetooth` group on Linux).
*   **BlueZ on Linux:** Ensure the BlueZ service is running (`sudo systemctl status bluetooth`). You might need `sudo` to run the application if it requires direct access to Bluetooth adapters, or configure udev rules.
*   **Windows Bluetooth:** The Windows Bluetooth implementation uses Win32 APIs. Ensure your Bluetooth adapter is enabled and drivers are up to date.
*   **Testing:** The application is designed to communicate with other BlueBeamNative instances or compatible BLE devices advertising the specified custom service and characteristics.
