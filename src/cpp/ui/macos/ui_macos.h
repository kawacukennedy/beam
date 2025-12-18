#pragma once

#include <Cocoa/Cocoa.h>
#include "../ui.h"

// FFI declarations
extern "C" {
    typedef void* CoreManagerPtr;
    CoreManagerPtr core_new();
    void core_free(CoreManagerPtr);
    void core_send_event(CoreManagerPtr, const char* event_json);
    char* core_recv_update(CoreManagerPtr);
    void core_free_string(char* str);
}

// Forward declarations
@class AppDelegate;
@class WelcomeViewController;
@class DeviceDiscoveryViewController;
@class PairingViewController;
@class ChatViewController;
@class FileTransferViewController;
@class SettingsViewController;

class MacUI : public UI {
public:
    MacUI();
    ~MacUI();

    void run() override;

    // Methods to switch screens
    void showWelcome();
    void showDeviceDiscovery();
    void showPairing(const std::string& deviceId);
    void showChat(const std::string& sessionId);
    void showFileTransfer(const std::string& transferId);
    void showSettings();

    // Send event to core
    void sendEvent(const std::string& eventJson);

    // Poll for updates
    void pollUpdates();

private:
    NSApplication* app;
    NSWindow* window;
    AppDelegate* delegate;
    CoreManagerPtr core;

    WelcomeViewController* welcomeVC;
    DeviceDiscoveryViewController* discoveryVC;
    PairingViewController* pairingVC;
    ChatViewController* chatVC;
    FileTransferViewController* transferVC;
    SettingsViewController* settingsVC;

    NSTimer* updateTimer;
};