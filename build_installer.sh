#!/bin/bash

# This script builds the BlueConnect Desktop installer for all platforms.
#
# Prerequisites:
# 1. Qt Installer Framework is installed.
# 2. The 'binarycreator' tool from the framework is in the system's PATH.
# 3. The application binaries (e.g., BlueConnect.exe, BlueConnect.app) are
#    placed in the correct 'data' directory before running this script.

# --- Configuration ---
QT_IFW_PATH="/path/to/your/Qt/Installer/Framework/4.1.1" # CHANGE THIS
CONFIG_FILE="installer_src/config.xml"
PACKAGES_DIR="installer_src/packages"
OUTPUT_DIR="installers"
APP_NAME="BlueConnectDesktopInstaller"

# --- Platform-specific data ---
# Before running, you must place your compiled application into these directories.
# For example:
# - installer_src/packages/com.yourcompany.blueconnect.application/data/bin/BlueConnect.exe
# - installer_src/packages/com.yourcompany.blueconnect.application/data/BlueConnect.app
# - installer_src/packages/com.yourcompany.blueconnect.application/data/bin/BlueConnect

# --- Build Function ---
build() {
    local platform=$1
    local output_name="$APP_NAME-$platform"

    echo "Building installer for $platform..."

    # Run binarycreator
    $QT_IFW_PATH/bin/binarycreator --verbose -c $CONFIG_FILE -p $PACKAGES_DIR $output_name

    if [ $? -eq 0 ]; then
        echo "Successfully created installer: $output_name"
    else
        echo "Error creating installer for $platform."
        exit 1
    fi
}

# --- Main Execution ---

# Create output directory
mkdir -p $OUTPUT_DIR

# Build for each platform
# Note: You need to run this on the respective OS to get a native installer.
# Cross-compilation of installers is not supported by default.

if [[ "$(uname)" == "Darwin" ]]; then
    build "mac"
elif [[ "$(uname -o)" == "Msys" || "$(uname -o)" == "Cygwin" ]]; then
    build "windows"
elif [[ "$(uname)" == "Linux" ]]; then
    build "linux"
fi

# --- Post-build Steps (Signing, Notarization) ---
# On macOS:
# codesign --sign "Your Developer ID" installers/BlueConnectDesktopInstaller-mac.app
# xcrun notarytool submit installers/BlueConnectDesktopInstaller-mac.dmg --keychain-profile "Your-Profile"

# On Windows:
# signtool sign /f YourCert.pfx /p YourPassword installers/BlueConnectDesktopInstaller-windows.exe

echo "Build process finished."
