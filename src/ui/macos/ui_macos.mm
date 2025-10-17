#import <Cocoa/Cocoa.h>
#include "ui_macos.h"
#import <Cocoa/Cocoa.h>
#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include "database/database.h"
#include "bluetooth/bluetooth.h"
#include "crypto/crypto.h"
#include "messaging/messaging.h"
#include "file_transfer/file_transfer.h"
#include "settings/settings.h"

static Database* db = nullptr;
static Bluetooth* bt = nullptr;
static Crypto* crypto = nullptr;
static Messaging* messaging = nullptr;
static FileTransfer* ft = nullptr;
static Settings* settings = nullptr;

static std::string selected_device_id;
static std::string current_conversation_id;
static bool first_run = true;

std::string generate_id() {
    return "id_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

@interface BlueBeamAppDelegate : NSObject <NSApplicationDelegate, NSTableViewDataSource, NSTableViewDelegate, NSToolbarDelegate>
@property (strong) NSWindow* window;
@property (strong) NSTextField* statusLabel;
@property (strong) NSButton* scanButton;
@property (strong) NSTableView* deviceTable;
@property (strong) NSMutableArray* deviceNames;
@property (strong) NSTextView* chatView;
@property (strong) NSTextField* messageField;
@property (strong) NSProgressIndicator* progressBar;
@property (strong) NSWindow* splashWindow;
@end

@implementation BlueBeamAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Initialize backends
    db = new Database();
    bt = new Bluetooth();
    crypto = new Crypto();
    messaging = new Messaging(*crypto);
    ft = new FileTransfer(*crypto);
    settings = new Settings();

    // Set up callbacks
    bt->set_receive_callback([self](const std::string& device_id, const std::vector<uint8_t>& data) {
        messaging->receive_data(device_id, data);
        ft->receive_packet(device_id, data);
    });
    messaging->set_bluetooth_sender([](const std::string& device_id, const std::vector<uint8_t>& data) {
        return bt->send_data(device_id, data);
    });
    messaging->set_message_callback([self](const std::string& id, const std::string& conversation_id,
                                           const std::string& sender_id, const std::string& receiver_id,
                                           const std::vector<uint8_t>& content, MessageStatus status) {
        [self onMessageReceived:[NSString stringWithUTF8String:id.c_str()] conversation:[NSString stringWithUTF8String:conversation_id.c_str()] sender:[NSString stringWithUTF8String:sender_id.c_str()] receiver:[NSString stringWithUTF8String:receiver_id.c_str()] content:[NSData dataWithBytes:content.data() length:content.size()] status:status];
    });
    ft->set_data_sender([](const std::string& device_id, const std::vector<uint8_t>& data) {
        return bt->send_data(device_id, data);
    });
    bt->set_disconnect_callback([self](const std::string& device_id) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self onDeviceDisconnected:device_id];
        });
    });
    ft->set_incoming_file_callback([self](const std::string& filename, uint64_t size, auto response) {
        dispatch_async(dispatch_get_main_queue(), ^{
            NSAlert* alert = [[NSAlert alloc] init];
            [alert setMessageText:@"Incoming file"];
            [alert setInformativeText:[NSString stringWithFormat:@"Receive file %s (%llu bytes)?", filename.c_str(), size]];
            [alert addButtonWithTitle:@"Accept"];
            [alert addButtonWithTitle:@"Reject"];
            if ([alert runModal] == NSAlertFirstButtonReturn) {
                NSSavePanel* panel = [NSSavePanel savePanel];
                [panel setNameFieldStringValue:[NSString stringWithUTF8String:filename.c_str()]];
                if ([panel runModal] == NSModalResponseOK) {
                    std::string save_path = [[[panel URL] path] UTF8String];
                    response(true, save_path);
                } else {
                    response(false, "");
                }
            } else {
                response(false, "");
            }
        });
    });

    // First time use flow
    if (first_run) {
        [self showSplashScreen];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 300 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
            [self requestBluetoothPermissions];
        });
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2000 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
            [self showOnboarding];
            [self.splashWindow close];
            [self createMainUI];
            first_run = false;
        });
    } else {
        [self createMainUI];
    }
}





- (void)showProfileSetup {
    NSWindow* profileWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 400, 300)
                                                          styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                                                            backing:NSBackingStoreBuffered
                                                              defer:NO];
    [profileWindow setTitle:@"Set Up Profile"];
    [profileWindow center];

    NSTextField* nameField = [[NSTextField alloc] initWithFrame:NSMakeRect(50, 200, 300, 24)];
    [nameField setPlaceholderString:@"Enter your name"];
    [profileWindow.contentView addSubview:nameField];

    NSButton* continueBtn = [[NSButton alloc] initWithFrame:NSMakeRect(150, 100, 100, 32)];
    [continueBtn setTitle:@"Continue"];
    [continueBtn setTarget:self];
    [continueBtn setAction:@selector(profileSetupDone:)];
    [profileWindow.contentView addSubview:continueBtn];

    [profileWindow makeKeyAndOrderFront:nil];
}

- (void)profileSetupDone:(id)sender {
    // Save profile
    [self showTutorialOverlay];
}

- (void)showTutorialOverlay {
    // Show tutorial overlay
    NSWindow* tutorial = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 600, 400)
                                                     styleMask:NSWindowStyleMaskBorderless
                                                       backing:NSBackingStoreBuffered
                                                         defer:NO];
    [tutorial setBackgroundColor:[NSColor colorWithWhite:0 alpha:0.8]];
    [tutorial center];
    NSTextField* tutorialText = [[NSTextField alloc] initWithFrame:NSMakeRect(50, 150, 500, 100)];
    [tutorialText setStringValue:@"Welcome to BlueBeam!\n\nUse the sidebar to discover devices and start chatting."];
    [tutorialText setEditable:NO];
    [tutorialText setBordered:NO];
    [tutorialText setBackgroundColor:[NSColor clearColor]];
    [tutorial.contentView addSubview:tutorialText];

    NSButton* gotItBtn = [[NSButton alloc] initWithFrame:NSMakeRect(250, 50, 100, 32)];
    [gotItBtn setTitle:@"Got it!"];
    [gotItBtn setTarget:self];
    [gotItBtn setAction:@selector(tutorialDone:)];
    [tutorial.contentView addSubview:gotItBtn];

    [tutorial makeKeyAndOrderFront:nil];
}

- (void)tutorialDone:(id)sender {
    first_run = false;
    [self createMainWindow];
}

- (void)createMainWindow {
    // Create main window
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1000, 700)
                                               styleMask:(NSWindowStyleMaskTitled |
                                                          NSWindowStyleMaskClosable |
                                                          NSWindowStyleMaskMiniaturizable |
                                                          NSWindowStyleMaskResizable)
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
    [self.window setTitle:@"BlueBeam"];
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];

    // Create toolbar
    NSToolbar* toolbar = [[NSToolbar alloc] initWithIdentifier:@"BlueBeamToolbar"];
    [toolbar setDelegate:self];
    [self.window setToolbar:toolbar];

    // Create split view
    NSSplitView* splitView = [[NSSplitView alloc] initWithFrame:[self.window.contentView bounds]];
    [splitView setVertical:YES];
    [self.window.contentView addSubview:splitView];

    // Sidebar
    NSView* sidebar = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 200, 700)];
    [sidebar setWantsLayer:YES];
    [sidebar.layer setBackgroundColor:[[NSColor lightGrayColor] CGColor]];

    NSButton* discoveryBtn = [[NSButton alloc] initWithFrame:NSMakeRect(10, 650, 180, 30)];
    [discoveryBtn setTitle:@"Device Discovery"];
    [discoveryBtn setTarget:self];
    [discoveryBtn setAction:@selector(showDiscovery:)];
    [sidebar addSubview:discoveryBtn];

    NSButton* chatBtn = [[NSButton alloc] initWithFrame:NSMakeRect(10, 610, 180, 30)];
    [chatBtn setTitle:@"Chat"];
    [chatBtn setTarget:self];
    [chatBtn setAction:@selector(showChat:)];
    [sidebar addSubview:chatBtn];

    NSButton* settingsBtn = [[NSButton alloc] initWithFrame:NSMakeRect(10, 570, 180, 30)];
    [settingsBtn setTitle:@"Settings"];
    [settingsBtn setTarget:self];
    [settingsBtn setAction:@selector(showSettings:)];
    [sidebar addSubview:settingsBtn];

    // Main content
    NSView* contentView = [[NSView alloc] initWithFrame:NSMakeRect(200, 0, 800, 700)];

    // Discovery view
    self.statusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 600, 760, 60)];
    [self.statusLabel setStringValue:@"Welcome to BlueBeam"];
    [self.statusLabel setEditable:NO];
    [self.statusLabel setBordered:NO];
    [self.statusLabel setBackgroundColor:[NSColor clearColor]];
    [self.statusLabel setFont:[NSFont systemFontOfSize:24]];
    [contentView addSubview:self.statusLabel];

    self.scanButton = [[NSButton alloc] initWithFrame:NSMakeRect(20, 550, 120, 32)];
    [self.scanButton setTitle:@"Scan Devices"];
    [self.scanButton setButtonType:NSButtonTypeMomentaryPushIn];
    [self.scanButton setBezelStyle:NSBezelStyleRounded];
    [self.scanButton setTarget:self];
    [self.scanButton setAction:@selector(scanDevices:)];
    [self.scanButton setAccessibilityLabel:@"Scan for nearby Bluetooth devices"];
    [self.scanButton setAccessibilityHelp:@"Click to search for available devices to connect to"];
    [contentView addSubview:self.scanButton];

    self.deviceTable = [[NSTableView alloc] initWithFrame:NSMakeRect(20, 100, 760, 400)];
    NSScrollView* scrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 100, 760, 400)];
    [scrollView setDocumentView:self.deviceTable];
    [contentView addSubview:scrollView];

    NSTableColumn* column = [[NSTableColumn alloc] initWithIdentifier:@"name"];
    [column setTitle:@"Device Name"];
    [self.deviceTable addTableColumn:column];
    [self.deviceTable setDataSource:self];
    self.deviceNames = [[NSMutableArray alloc] init];

    // Chat view (hidden initially)
    self.chatView = [[NSTextView alloc] initWithFrame:NSMakeRect(20, 100, 760, 500)];
    [self.chatView setHidden:YES];
    [contentView addSubview:self.chatView];

    self.messageField = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 50, 600, 30)];
    [self.messageField setHidden:YES];
    [contentView addSubview:self.messageField];

    NSButton* sendBtn = [[NSButton alloc] initWithFrame:NSMakeRect(640, 50, 80, 30)];
    [sendBtn setTitle:@"Send"];
    [sendBtn setHidden:YES];
    [sendBtn setTarget:self];
    [sendBtn setAction:@selector(sendMessage:)];
    [contentView addSubview:sendBtn];

    NSButton* fileBtn = [[NSButton alloc] initWithFrame:NSMakeRect(730, 50, 50, 30)];
    [fileBtn setTitle:@"File"];
    [fileBtn setHidden:YES];
    [fileBtn setTarget:self];
    [fileBtn setAction:@selector(sendFile:)];
    [contentView addSubview:fileBtn];

    [splitView addSubview:sidebar];
    [splitView addSubview:contentView];
    [splitView setPosition:200 ofDividerAtIndex:0];
}



- (void)scanDevices:(id)sender {
    [self.statusLabel setStringValue:@"Scanning for Bluetooth devices..."];
    bt->scan();
    // Wait for scan
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2000 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
        [self.statusLabel setStringValue:@"Scan complete"];
        auto devices = bt->get_discovered_devices();
        [self.deviceNames removeAllObjects];
        for (const auto& dev : devices) {
            [self.deviceNames addObject:[NSString stringWithUTF8String:dev.c_str()]];
        }
        [self.deviceTable reloadData];
    });
}

- (void)showDiscovery:(id)sender {
    [self.chatView setHidden:YES];
    [self.messageField setHidden:YES];
    [self.statusLabel setHidden:NO];
    [self.scanButton setHidden:NO];
    [self.deviceTable setHidden:NO];
}

- (void)showChat:(id)sender {
    [self.statusLabel setHidden:YES];
    [self.scanButton setHidden:YES];
    [self.deviceTable setHidden:YES];
    [self.chatView setHidden:NO];
    [self.messageField setHidden:NO];
}

 - (void)showSettings:(id)sender {
     NSWindow* settingsWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 400, 200)
                                                           styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                                                             backing:NSBackingStoreBuffered
                                                               defer:NO];
     [settingsWindow setTitle:@"Settings"];
     [settingsWindow center];

     NSTextField* nameLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 140, 80, 24)];
     [nameLabel setStringValue:@"User Name:"];
     [nameLabel setEditable:NO];
     [nameLabel setBordered:NO];
     [nameLabel setBackgroundColor:[NSColor clearColor]];
     [[settingsWindow contentView] addSubview:nameLabel];

     NSTextField* nameField = [[NSTextField alloc] initWithFrame:NSMakeRect(110, 140, 270, 24)];
     std::string name = settings->get_user_name();
     [nameField setStringValue:[NSString stringWithUTF8String:name.c_str()]];
     [nameField setTag:1]; // Tag for identification
     [[settingsWindow contentView] addSubview:nameField];

     NSButton* saveButton = [[NSButton alloc] initWithFrame:NSMakeRect(150, 50, 80, 32)];
     [saveButton setTitle:@"Save"];
     [saveButton setTarget:self];
     [saveButton setAction:@selector(saveSettings:)];
     [[settingsWindow contentView] addSubview:saveButton];

     NSButton* cancelButton = [[NSButton alloc] initWithFrame:NSMakeRect(250, 50, 80, 32)];
     [cancelButton setTitle:@"Cancel"];
     [cancelButton setTarget:self];
     [cancelButton setAction:@selector(cancelSettings:)];
     [[settingsWindow contentView] addSubview:cancelButton];

     [settingsWindow makeKeyAndOrderFront:nil];
 }

 - (void)saveSettings:(id)sender {
     NSWindow* window = [sender window];
     NSArray* subviews = [[window contentView] subviews];
     for (NSView* view in subviews) {
         if ([view isKindOfClass:[NSTextField class]] && [view tag] == 1) {
             NSTextField* field = (NSTextField*)view;
             std::string name = [[field stringValue] UTF8String];
             settings->set_user_name(name);
             settings->save();
             break;
         }
     }
     [window close];
 }

 - (void)cancelSettings:(id)sender {
     [[sender window] close];
 }





- (void)sendMessage:(id)sender {
    NSString* text = [self.messageField stringValue];
    if ([text length] == 0 || selected_device_id.empty()) return;
    std::string msg = [text UTF8String];
    std::vector<uint8_t> content(msg.begin(), msg.end());
    std::string id = generate_id();
    messaging->send_message(id, current_conversation_id, "self", selected_device_id, content, MessageStatus::SENT);
    [self.messageField setStringValue:@""];
    // Append to chat
    NSAttributedString* attrStr = [[NSAttributedString alloc] initWithString:[NSString stringWithFormat:@"Me: %@\n", text]];
    [[self.chatView textStorage] appendAttributedString:attrStr];
}

- (void)sendFile:(id)sender {
    if (selected_device_id.empty()) return;
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel setCanChooseFiles:YES];
    [panel setCanChooseDirectories:NO];
    [panel setAllowsMultipleSelection:NO];
    if ([panel runModal] == NSModalResponseOK) {
        NSURL* url = [[panel URLs] objectAtIndex:0];
        std::string path = [[url path] UTF8String];
        ft->send_file(path, selected_device_id,
                       [self](uint64_t sent, uint64_t total) {
                           // Update progress
                           dispatch_async(dispatch_get_main_queue(), ^{
                               double percent = total > 0 ? (double)sent / total * 100.0 : 0.0;
                               [self.progressBar setDoubleValue:percent];
                               [self.statusLabel setStringValue:[NSString stringWithFormat:@"Sending file: %llu/%llu", sent, total]];
                           });
                       },
                       [self](bool success, const std::string& error) {
                           dispatch_async(dispatch_get_main_queue(), ^{
                               [self.progressBar setDoubleValue:100.0];
                               if (success) {
                                   [self.statusLabel setStringValue:@"File sent successfully"];
                               } else {
                                   [self.statusLabel setStringValue:[NSString stringWithFormat:@"File send failed: %s", error.c_str()]];
                               }
                           });
                       });
    }
}

- (void)showSplashScreen {
    NSWindow* splash = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 400, 300) styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
    [splash center];
    NSTextField* label = [[NSTextField alloc] initWithFrame:NSMakeRect(150, 130, 100, 40)];
    [label setStringValue:@"BlueBeam"];
    [label setEditable:NO];
    [label setFont:[NSFont systemFontOfSize:24]];
    [[splash contentView] addSubview:label];
    [splash makeKeyAndOrderFront:nil];
    self.splashWindow = splash;
}

- (void)requestBluetoothPermissions {
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Bluetooth Permissions"];
    [alert setInformativeText:@"BlueBeam needs Bluetooth access to communicate with devices."];
    [alert runModal];
}

- (void)showOnboarding {
    NSWindow* onboarding = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 500, 400) styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable backing:NSBackingStoreBuffered defer:NO];
    [onboarding center];
    [onboarding setTitle:@"Welcome to BlueBeam"];
    NSTextField* text = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 20, 460, 360)];
    [text setStringValue:@"Welcome to BlueBeam!\n\n1. Scan for devices.\n2. Connect to a device.\n3. Start messaging or sending files.\n\nEnjoy secure Bluetooth communication."];
    [text setEditable:NO];
    [[onboarding contentView] addSubview:text];
    [onboarding makeKeyAndOrderFront:nil];
    // Close after 5 seconds
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        [onboarding close];
    });
}

- (void)onMessageReceived:(NSString*)messageId conversation:(NSString*)conversation_id
                   sender:(NSString*)sender_id receiver:(NSString*)receiver_id
                   content:(NSData*)contentData status:(MessageStatus)status {
    std::string convId = [conversation_id UTF8String];
    std::string sendId = [sender_id UTF8String];
    const uint8_t* bytes = (const uint8_t*)[contentData bytes];
    size_t length = [contentData length];
    std::vector<uint8_t> content(bytes, bytes + length);
    std::string msg(content.begin(), content.end());
    if (convId == current_conversation_id) {
        NSAttributedString* attrStr = [[NSAttributedString alloc] initWithString:[NSString stringWithFormat:@"%s: %s\n", sendId.c_str(), msg.c_str()]];
        [[self.chatView textStorage] appendAttributedString:attrStr];
    }
}

- (void)onDeviceDisconnected:(const std::string&)device_id {
    if (device_id == selected_device_id) {
        // Show reconnect banner
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Device disconnected"];
        [alert setInformativeText:@"The Bluetooth device has disconnected."];
        [alert runModal];
    }
}

- (void)createMainUI {
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 800, 600) styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:NO];
    [self.window center];
    [self.window setTitle:@"BlueBeam"];
    [self.window makeKeyAndOrderFront:nil];

    // Create device list
    self.deviceTable = [[NSTableView alloc] initWithFrame:NSMakeRect(0, 0, 200, 400)];
    NSTableColumn* column = [[NSTableColumn alloc] initWithIdentifier:@"Device"];
    [column setWidth:200];
    [self.deviceTable addTableColumn:column];
    [self.deviceTable setDataSource:self];
    [self.deviceTable setDelegate:self];
    NSScrollView* scrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(10, 10, 200, 400)];
    [scrollView setDocumentView:self.deviceTable];
    [[self.window contentView] addSubview:scrollView];

    // Create scan button
    self.scanButton = [[NSButton alloc] initWithFrame:NSMakeRect(10, 420, 100, 30)];
    [self.scanButton setTitle:@"Scan"];
    [self.scanButton setTarget:self];
    [self.scanButton setAction:@selector(scanDevices:)];
    [[self.window contentView] addSubview:self.scanButton];

    // Create connect button
    NSButton* connectButton = [[NSButton alloc] initWithFrame:NSMakeRect(120, 420, 90, 30)];
    [connectButton setTitle:@"Connect"];
    [connectButton setTarget:self];
    [connectButton setAction:@selector(connectToDevice:)];
    [[self.window contentView] addSubview:connectButton];

    // Create chat view
    self.chatView = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 570, 400)];
    [self.chatView setEditable:NO];
    NSScrollView* chatScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(220, 10, 570, 400)];
    [chatScroll setDocumentView:self.chatView];
    [[self.window contentView] addSubview:chatScroll];

    // Create message field
    self.messageField = [[NSTextField alloc] initWithFrame:NSMakeRect(220, 420, 470, 30)];
    [[self.window contentView] addSubview:self.messageField];

    // Create send button
    NSButton* sendButton = [[NSButton alloc] initWithFrame:NSMakeRect(650, 420, 70, 30)];
    [sendButton setTitle:@"Send"];
    [sendButton setTarget:self];
    [sendButton setAction:@selector(sendMessage:)];
    [[self.window contentView] addSubview:sendButton];

    // Create send file button
    NSButton* sendFileButton = [[NSButton alloc] initWithFrame:NSMakeRect(730, 420, 60, 30)];
    [sendFileButton setTitle:@"File"];
    [sendFileButton setTarget:self];
    [sendFileButton setAction:@selector(sendFile:)];
    [[self.window contentView] addSubview:sendFileButton];

    // Create status label
    self.statusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 480, 780, 20)];
    [self.statusLabel setEditable:NO];
    [self.statusLabel setStringValue:@"Ready"];
    [[self.window contentView] addSubview:self.statusLabel];

    // Create progress bar
    self.progressBar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(10, 460, 780, 20)];
    [self.progressBar setStyle:NSProgressIndicatorStyleBar];
    [self.progressBar setIndeterminate:NO];
    [self.progressBar setMinValue:0.0];
    [self.progressBar setMaxValue:100.0];
    [self.progressBar setDoubleValue:0.0];
    [[self.window contentView] addSubview:self.progressBar];

    self.deviceNames = [[NSMutableArray alloc] init];
}

 - (void)connectToDevice:(id)sender {
     NSInteger row = [self.deviceTable selectedRow];
     if (row >= 0) {
         NSString* deviceName = [self.deviceNames objectAtIndex:row];
         std::string name = [deviceName UTF8String];
         selected_device_id = bt->get_device_id_from_name(name);
         if (bt->connect(selected_device_id)) {
             [self.statusLabel setStringValue:[NSString stringWithFormat:@"Connected to %@", deviceName]];
             current_conversation_id = selected_device_id; // Simple conversation id
             [self loadMessageHistory];
         } else {
             [self.statusLabel setStringValue:@"Failed to connect"];
         }
     }
 }

 - (void)loadMessageHistory {
     if (current_conversation_id.empty()) return;

     auto messages = db->get_messages(current_conversation_id);
     [[self.chatView textStorage] setAttributedString:[[NSAttributedString alloc] initWithString:@""]];
     for (const auto& msg : messages) {
         std::string display = msg.sender_id + ": " + std::string(msg.content.begin(), msg.content.end()) + "\n";
         NSAttributedString* attrStr = [[NSAttributedString alloc] initWithString:[NSString stringWithUTF8String:display.c_str()]];
         [[self.chatView textStorage] appendAttributedString:attrStr];
     }
 }

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return [self.deviceNames count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row {
    return [self.deviceNames objectAtIndex:row];
}

@end

class UI::Impl {
public:
    BlueBeamAppDelegate* delegate;

    Impl() {
        [NSApplication sharedApplication];
        delegate = [[BlueBeamAppDelegate alloc] init];
        [NSApp setDelegate:delegate];
    }

    ~Impl() {
        delete db;
        delete bt;
        delete crypto;
        delete messaging;
        delete ft;
        delete settings;
    }

    void run() {
        [NSApp run];
    }
};

UI::UI() : pimpl(std::make_unique<Impl>()) {}
UI::~UI() = default;

void UI::run() {
    pimpl->run();
}