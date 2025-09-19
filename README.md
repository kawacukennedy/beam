# BlueLink Manager

**Cross-platform Bluetooth manager and chat/file transfer app for laptops using C++17 and Qt 6. Supports device discovery, pairing, secure chat, file transfer, and full offline operation with low-latency performance.**

## Table of Contents

- [Features](#features)
- [Tech Stack](#tech-stack)
- [UI/UX Specifications](#uiux-specifications)
- [Performance Targets](#performance-targets)
- [Logging](#logging)
- [Deployment](#deployment)
- [Bluetooth Protocol](#bluetooth-protocol)
- [Building and Running](#building-and-running)
- [License](#license)

## Features

BlueLink Manager offers a robust set of features designed for seamless Bluetooth interaction:

-   **Bluetooth Management**: Handles device discovery, connection, pairing, and data transfer across Windows, macOS, and Linux.
-   **Secure Chat**: Provides encrypted, low-latency chat functionality between paired devices.
-   **File Transfer**: Facilitates secure and efficient file transfers with resume capabilities and robust error handling.
-   **Offline Operation**: Designed for full functionality without requiring an internet connection.
-   **Intuitive UI**: A modern, responsive user interface built with Qt Quick/QML.

## Tech Stack

-   **Language**: C++17
-   **UI Framework**: Qt 6 (Qt Quick/QML + Qt Widgets)
-   **Bluetooth Implementation**:
    -   **Windows**: WinRT Bluetooth API via C++/WinRT
    -   **macOS**: CoreBluetooth via Objective-C++ bridge
    -   **Linux**: BlueZ D-Bus API via QtDBus
-   **Threading Model**:
    -   **Scan**: QThreadPool (max 2 threads, async)
    -   **Pairing**: WorkerThread (signals: success, error)
    -   **Transfer**: ThreadPerFile (max 4 concurrent, non-blocking UI)
    -   **UI**: Qt main thread
-   **Encryption**: AES-CCM 128-bit for BLE, L2CAP for file transfer
-   **Retry Policy**: Exponential backoff, max 3 retries per chunk
-   **Database**: SQLite encrypted
    -   **Schema**:
        -   `devices`: id, name, mac, paired, last_seen, signal_strength
        -   `chats`: id, device_id, timestamp, sender, message_type, content
        -   `file_transfers`: id, device_id, filename, size, progress, status, timestamp
-   **Build System**: CMake 3.28+ with Ninja
-   **Packaging**:
    -   **Windows**: MSIX/NSIS installer, code signed
    -   **macOS**: DMG notarized and signed
    -   **Linux**: AppImage, .deb, .rpm, Flatpak

## UI/UX Specifications

### Global Style

-   **Grid Unit**: 8px
-   **Corner Radius**: 12px
-   **Shadows**:
    -   Low: `0 1px 3px rgba(0,0,0,0.12)`
    -   Medium: `0 4px 6px rgba(0,0,0,0.16)`
    -   High: `0 10px 20px rgba(0,0,0,0.25)`
-   **Fonts**:
    -   Family: System Default
    -   Sizes: h1 (24px), h2 (18px), body (14px), caption (12px)
    -   Weights: h1 (600), h2 (500), body (400), caption (400)
-   **Colors**:
    -   **Light Theme**:
        -   Background: `#FAFAFA`
        -   Surface: `#FFFFFF`
        -   Primary: `#0078D7`
        -   Secondary: `#00A1F1`
        -   Text Primary: `#1C1C1C`
        -   Text Secondary: `#666666`
        -   Border: `#E0E0E0`
        -   Error: `#D32F2F`
        -   Success: `#2E7D32`
    -   **Dark Theme**:
        -   Background: `#1E1E1E`
        -   Surface: `#2C2C2C`
        -   Primary: `#0A84FF`
        -   Secondary: `#0A9EFF`
        -   Text Primary: `#FFFFFF`
        -   Text Secondary: `#AAAAAA`
        -   Border: `#333333`
        -   Error: `#FF6B6B`
        -   Success: `#4CAF50`

### Components

-   **Button**:
    -   Padding: `12px 20px`
    -   Radius: `8px`
    -   States:
        -   Default: Background (primary), Text (`#FFF`)
        -   Hover: Background brightness `+8%`, Shadow (low)
        -   Pressed: Background brightness `-10%`, Shadow (none)
        -   Disabled: Opacity `0.5`
    -   Animation: `150ms ease-in-out`
-   **Input Field**:
    -   Height: `40px`
    -   Padding: `10px`
    -   Border: `1px solid #CCC`
    -   Focus: Border `2px solid primary`, Shadow (glow)
    -   Error: Border `2px solid error`, Animation (`shake 200ms`)
-   **Toast Notification**:
    -   Position: Bottom-right
    -   Duration: `4000ms`
    -   Animations: Entry (`slide-in 200ms`), Exit (`fade-out 150ms`)
-   **Sidebar**:
    -   Width: `240px`
    -   Padding: `16px`
    -   Hover: Background (`#E6F0FF`)
    -   Collapse Animation: `200ms cubic-bezier(0.4,0,0.2,1)`
-   **Top Bar**:
    -   Height: `60px`
    -   Elements: Bluetooth toggle, Search bar, Profile avatar

### Screens

-   **Dashboard**:
    -   Layout: GridLayout
    -   Components: Adapter Card, Recent Devices Card
-   **Device List**:
    -   Layout: ListView
    -   Card Size: `220px x 100px`
    -   Fields: Name, MAC, RSSI, Paired
    -   Actions: Connect, Disconnect, Pair, Unpair
-   **Chat**:
    -   Layout: ColumnLayout
    -   Left Panel: Width `240px`, Avatars `40px`
    -   Main Panel:
        -   Outgoing Bubble: Background (`#0078D7`), Text (`#FFF`), Radius `16px`, Animation (`fade+slide 150ms`)
        -   Incoming Bubble: Background (`#E6E6E6`), Text (`#000`), Radius `16px`, Animation (`fade+slide 150ms`)
        -   Status Icons: Sending (spinner), Sent (✓), Read (✓✓ blue)
        -   Typing Indicator: `3-dot bounce 1s loop`
    -   Input Field: Height `40px`, Buttons (emoji, file_send)
-   **File Transfer**:
    -   Layout: TableView
    -   Columns: File, Size, Device, Progress, Status
    -   Progress Bar: Height `8px`, Animation (striped)
    -   Logic: Chunk size `8MB`, Retry (`3 attempts exponential backoff`), Resume (`checkpoint by offset`)
-   **Settings**:
    -   Layout: StackLayout
    -   Sections: General, Bluetooth Preferences, Advanced
    -   Fields:
        -   General: Language, Theme, Startup Behavior
        -   Bluetooth Preferences: Auto-Connect, Device Priority
        -   Advanced: Debug Logging, Developer Mode

### UX Flows

-   **Pairing**: Scan → select device → passkey dialog → confirm → success toast
-   **Chat**: Select paired device → load history → type → send → bubble animates → delivery status updates
-   **File Transfer**: Select file → confirm → chunked transfer → receiver saves → log in SQLite

### Error Handling

-   **Minor**: Toast retry
-   **Major**: Modal retry/cancel
-   **Critical**: Reset stack + JSON log

### Accessibility

-   **Keyboard**: Tab/Arrow navigation
-   **Screen Reader**: Qt Accessibility bridge
-   **Contrast**: WCAG AA
-   **Scaling**: 100–200%

## Performance Targets

-   **Startup**: `<2s`
-   **Idle Memory**: `<60MB`
-   **UI Latency**: `<16ms/frame`
-   **Scan Time**: `≤5s`
-   **Transfer Speed**: `≥2Mbps BT5`

## Logging

-   **Format**: JSON
-   **Levels**: INFO, DEBUG, WARN, ERROR
-   **Rotation**: `10MB/file`, keep 5

## Deployment

-   **CI/CD**: GitHub Actions cross-platform
-   **Update Mechanism**: In-app delta
-   **Release Channels**: Stable, Beta

## Bluetooth Protocol

-   **Scan Interval**: `500ms`
-   **Connect Timeout**: `10000ms`
-   **Pairing Methods**: JustWorks, PasskeyEntry, NumericComparison
-   **Encryption**: AES-CCM 128-bit
-   **Chunk Size**: `8MB`
-   **Retry Policy**: Exponential backoff, 3 retries

## Building and Running

To build and run the BlueLink Manager application, follow these steps:

1.  **Clone the repository**:
    ```bash
    git clone https://github.com/kawacukennedy/beam.git
    cd beam/BlueBeamNative
    ```

2.  **Create a build directory and configure CMake**:
    ```bash
    mkdir build
    cd build
    cmake ..
    ```
    *Note: Ensure you have CMake 3.28+ and Qt 6 installed and configured in your system's PATH.*

3.  **Build the application**:
    ```bash
    cmake --build .
    ```

4.  **Run the application**:
    ```bash
    ./BlueLinkManager_GUI
    ```
    *If the UI does not appear, ensure you are running in a graphical environment and check the console output for any errors.*

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.