#include "ui_macos.h"
#include <iostream>
#include <string>
#include <vector>

// AppDelegate
@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, assign) MacUI* ui;
@end

@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Setup
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}
@end

// Utility functions
NSColor* colorFromHex(NSString* hex) {
    unsigned int r, g, b;
    NSScanner* scanner = [NSScanner scannerWithString:hex];
    [scanner scanString:@"#" intoString:nil];
    [scanner scanHexInt:&r];
    [scanner scanHexInt:&g];
    [scanner scanHexInt:&b];
    return [NSColor colorWithRed:r/255.0 green:g/255.0 blue:b/255.0 alpha:1.0];
}

NSFont* primaryFont(CGFloat size) {
    return [NSFont fontWithName:@"SFProText-Regular" size:size] ?: [NSFont systemFontOfSize:size];
}

NSFont* headingFont(CGFloat size) {
    return [NSFont fontWithName:@"SFProDisplay-Bold" size:size] ?: [NSFont boldSystemFontOfSize:size];
}

// WelcomeViewController
@interface WelcomeViewController : NSViewController
@property (nonatomic, assign) MacUI* ui;
@end

@implementation WelcomeViewController
- (void)viewDidLoad {
    [super viewDidLoad];

    self.view.wantsLayer = YES;
    self.view.layer.backgroundColor = colorFromHex(@"#FFFFFF").CGColor;

    // Title
    NSTextField* titleLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(50, 300, 300, 40)];
    titleLabel.stringValue = @"Welcome to BlueBeam";
    titleLabel.font = headingFont(32);
    titleLabel.textColor = colorFromHex(@"#020617");
    titleLabel.editable = NO;
    titleLabel.bezeled = NO;
    titleLabel.backgroundColor = [NSColor clearColor];
    [self.view addSubview:titleLabel];

    // Description
    NSTextField* descLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(50, 250, 300, 40)];
    descLabel.stringValue = @"Secure, offline messaging and file transfer.";
    descLabel.font = primaryFont(16);
    descLabel.editable = NO;
    descLabel.bezeled = NO;
    descLabel.backgroundColor = [NSColor clearColor];
    [self.view addSubview:descLabel];

    // Permission button
    NSButton* button = [[NSButton alloc] initWithFrame:NSMakeRect(50, 200, 150, 30)];
    button.title = @"Grant Bluetooth Access";
    button.target = self;
    button.action = @selector(grantPermission:);
    [self.view addSubview:button];
}

- (void)grantPermission:(id)sender {
    // Request Bluetooth permission
    // For now, proceed to discovery
    self.ui->showDeviceDiscovery();
}
@end

// DeviceDiscoveryViewController
@interface DeviceDiscoveryViewController : NSViewController <NSTableViewDataSource, NSTableViewDelegate>
@property (nonatomic, assign) MacUI* ui;
@property (nonatomic, strong) NSTableView* tableView;
@property (nonatomic, strong) NSMutableArray* devices;
@property (nonatomic, strong) NSButton* refreshButton;
@end

@implementation DeviceDiscoveryViewController
- (void)viewDidLoad {
    [super viewDidLoad];

    self.view.wantsLayer = YES;
    self.view.layer.backgroundColor = colorFromHex(@"#F1F5F9").CGColor;

    self.devices = [NSMutableArray array];

    // Title
    NSTextField* titleLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(50, 350, 300, 30)];
    titleLabel.stringValue = @"Discover Devices";
    titleLabel.font = headingFont(24);
    titleLabel.editable = NO;
    titleLabel.bezeled = NO;
    titleLabel.backgroundColor = [NSColor clearColor];
    [self.view addSubview:titleLabel];

    // Table
    NSScrollView* scrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(50, 100, 300, 200)];
    self.tableView = [[NSTableView alloc] initWithFrame:scrollView.bounds];
    NSTableColumn* column = [[NSTableColumn alloc] initWithIdentifier:@"Device"];
    column.title = @"Devices";
    [self.tableView addTableColumn:column];
    self.tableView.dataSource = self;
    self.tableView.delegate = self;
    scrollView.documentView = self.tableView;
    [self.view addSubview:scrollView];

    // Refresh button
    self.refreshButton = [[NSButton alloc] initWithFrame:NSMakeRect(50, 50, 100, 30)];
    self.refreshButton.title = @"Refresh";
    self.refreshButton.target = self;
    self.refreshButton.action = @selector(refresh:);
    [self.view addSubview:self.refreshButton];

    // Start discovery
    [self startDiscovery];
}

- (void)startDiscovery {
    self.refreshButton.title = @"Scanning...";
    // Send event
    std::string event = R"({"StartDiscovery": null})";
    self.ui->sendEvent(event);
}

- (void)refresh:(id)sender {
    [self startDiscovery];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return self.devices.count;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row {
    NSTextField* cell = [tableView makeViewWithIdentifier:@"DeviceCell" owner:self];
    if (!cell) {
        cell = [[NSTextField alloc] initWithFrame:NSZeroRect];
        cell.identifier = @"DeviceCell";
        cell.editable = NO;
        cell.bezeled = NO;
        cell.backgroundColor = [NSColor clearColor];
    }
    NSDictionary* device = self.devices[row];
    cell.stringValue = device[@"name"];
    return cell;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification {
    NSInteger row = self.tableView.selectedRow;
    if (row >= 0) {
        NSDictionary* device = self.devices[row];
        NSString* deviceId = device[@"id"];
        self.ui->showPairing(std::string([deviceId UTF8String]));
    }
}

// Add device method
- (void)addDevice:(NSString*)deviceId name:(NSString*)name {
    [self.devices addObject:@{@"id": deviceId, @"name": name}];
    [self.tableView reloadData];
    self.refreshButton.title = @"Refresh";
}
@end

// PairingViewController
@interface PairingViewController : NSViewController
@property (nonatomic, assign) MacUI* ui;
@property (nonatomic, strong) NSString* deviceId;
@property (nonatomic, strong) NSTextField* pinLabel;
@property (nonatomic, strong) NSButton* confirmButton;
@end

@implementation PairingViewController
- (instancetype)initWithDeviceId:(NSString*)deviceId ui:(MacUI*)ui {
    self = [super init];
    if (self) {
        self.deviceId = deviceId;
        self.ui = ui;
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    self.view.wantsLayer = YES;
    self.view.layer.backgroundColor = colorFromHex(@"#FFFFFF").CGColor;

    // Title
    NSTextField* titleLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(50, 300, 300, 30)];
    titleLabel.stringValue = @"Pairing";
    titleLabel.font = headingFont(24);
    titleLabel.editable = NO;
    titleLabel.bezeled = NO;
    titleLabel.backgroundColor = [NSColor clearColor];
    [self.view addSubview:titleLabel];

    // PIN display
    self.pinLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(50, 250, 200, 30)];
    self.pinLabel.stringValue = @"PIN: 123456"; // Placeholder
    self.pinLabel.font = primaryFont(20);
    self.pinLabel.editable = NO;
    self.pinLabel.bezeled = NO;
    self.pinLabel.backgroundColor = [NSColor clearColor];
    [self.view addSubview:self.pinLabel];

    // Confirm button
    self.confirmButton = [[NSButton alloc] initWithFrame:NSMakeRect(50, 200, 100, 30)];
    self.confirmButton.title = @"Confirm";
    self.confirmButton.target = self;
    self.confirmButton.action = @selector(confirm:);
    [self.view addSubview:self.confirmButton];
}

- (void)confirm:(id)sender {
    // Send pair event
    std::string event = std::string("{\"PairWithDevice\": \"") + [self.deviceId UTF8String] + "\"}";
    self.ui->sendEvent(event);
    // Proceed to chat or something
    self.ui->showChat("session_id"); // Placeholder
}
@end

// ChatViewController
@interface ChatViewController : NSViewController <NSTextFieldDelegate>
@property (nonatomic, assign) MacUI* ui;
@property (nonatomic, strong) NSString* sessionId;
@property (nonatomic, strong) NSTextView* messageView;
@property (nonatomic, strong) NSTextField* composer;
@property (nonatomic, strong) NSButton* sendButton;
@property (nonatomic, strong) NSMutableArray* messages;
@end

@implementation ChatViewController
- (instancetype)initWithSessionId:(NSString*)sessionId ui:(MacUI*)ui {
    self = [super init];
    if (self) {
        self.sessionId = sessionId;
        self.ui = ui;
        self.messages = [NSMutableArray array];
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    self.view.wantsLayer = YES;
    self.view.layer.backgroundColor = colorFromHex(@"#F1F5F9").CGColor;

    // Message list
    NSScrollView* scrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(50, 100, 300, 200)];
    self.messageView = [[NSTextView alloc] initWithFrame:scrollView.bounds];
    self.messageView.editable = NO;
    scrollView.documentView = self.messageView;
    [self.view addSubview:scrollView];

    // Composer
    self.composer = [[NSTextField alloc] initWithFrame:NSMakeRect(50, 50, 200, 30)];
    self.composer.placeholderString = @"Type a message...";
    self.composer.delegate = self;
    [self.view addSubview:self.composer];

    // Send button
    self.sendButton = [[NSButton alloc] initWithFrame:NSMakeRect(260, 50, 60, 30)];
    self.sendButton.title = @"Send";
    self.sendButton.target = self;
    self.sendButton.action = @selector(sendMessage:);
    [self.view addSubview:self.sendButton];
}

- (void)sendMessage:(id)sender {
    NSString* message = self.composer.stringValue;
    if (message.length > 0) {
        std::string event = std::string("{\"SendMessage\": [\"") + [self.sessionId UTF8String] + "\", \"" + [message UTF8String] + "\"]}";
        self.ui->sendEvent(event);
        self.composer.stringValue = @"";
    }
}

- (void)addMessage:(NSString*)message {
    [self.messages addObject:message];
    self.messageView.string = [self.messages componentsJoinedByString:@"\n"];
}
@end

// FileTransferViewController
@interface FileTransferViewController : NSViewController
@property (nonatomic, assign) MacUI* ui;
@property (nonatomic, strong) NSString* transferId;
@property (nonatomic, strong) NSProgressIndicator* progressBar;
@property (nonatomic, strong) NSButton* pauseButton;
@property (nonatomic, strong) NSButton* resumeButton;
@end

@implementation FileTransferViewController
- (instancetype)initWithTransferId:(NSString*)transferId ui:(MacUI*)ui {
    self = [super init];
    if (self) {
        self.transferId = transferId;
        self.ui = ui;
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    self.view.wantsLayer = YES;
    self.view.layer.backgroundColor = colorFromHex(@"#FFFFFF").CGColor;

    // Progress bar
    self.progressBar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(50, 200, 300, 20)];
    self.progressBar.style = NSProgressIndicatorBarStyle;
    self.progressBar.minValue = 0.0;
    self.progressBar.maxValue = 1.0;
    [self.view addSubview:self.progressBar];

    // Pause button
    self.pauseButton = [[NSButton alloc] initWithFrame:NSMakeRect(50, 150, 100, 30)];
    self.pauseButton.title = @"Pause";
    self.pauseButton.target = self;
    self.pauseButton.action = @selector(pause:);
    [self.view addSubview:self.pauseButton];

    // Resume button
    self.resumeButton = [[NSButton alloc] initWithFrame:NSMakeRect(160, 150, 100, 30)];
    self.resumeButton.title = @"Resume";
    self.resumeButton.target = self;
    self.resumeButton.action = @selector(resume:);
    [self.view addSubview:self.resumeButton];
}

- (void)pause:(id)sender {
    // Send cancel or pause event
    std::string event = std::string("{\"CancelTransfer\": \"") + [self.transferId UTF8String] + "\"}";
    self.ui->sendEvent(event);
}

- (void)resume:(id)sender {
    // Resume logic
}

- (void)updateProgress:(double)progress {
    self.progressBar.doubleValue = progress;
}
@end

// SettingsViewController
@interface SettingsViewController : NSViewController
@property (nonatomic, assign) MacUI* ui;
@end

@implementation SettingsViewController
- (void)viewDidLoad {
    [super viewDidLoad];

    self.view.wantsLayer = YES;
    self.view.layer.backgroundColor = colorFromHex(@"#F1F5F9").CGColor;

    // Toggles and options
    NSTextField* label = [[NSTextField alloc] initWithFrame:NSMakeRect(50, 300, 200, 30)];
    label.stringValue = @"Settings";
    label.font = headingFont(24);
    label.editable = NO;
    label.bezeled = NO;
    label.backgroundColor = [NSColor clearColor];
    [self.view addSubview:label];

    // Add toggles here
}
@end

// MacUI implementation
MacUI::MacUI() : core(nullptr), updateTimer(nullptr) {
    core = core_new();
    if (!core) {
        std::cerr << "Failed to create core" << std::endl;
        return;
    }

    app = [NSApplication sharedApplication];
    delegate = [[AppDelegate alloc] init];
    delegate.ui = this;
    app.delegate = delegate;

    window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 400, 400)
                                         styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                                           backing:NSBackingStoreBuffered
                                             defer:NO];
    [window center];
    window.title = @"BlueBeam";

    welcomeVC = [[WelcomeViewController alloc] init];
    welcomeVC.ui = this;

    discoveryVC = [[DeviceDiscoveryViewController alloc] init];
    discoveryVC.ui = this;

    pairingVC = nil;
    chatVC = nil;
    transferVC = nil;
    settingsVC = [[SettingsViewController alloc] init];
    settingsVC.ui = this;

    showWelcome();
}

MacUI::~MacUI() {
    if (core) {
        core_free(core);
    }
}

void MacUI::run() {
    updateTimer = [NSTimer scheduledTimerWithTimeInterval:0.1 target:this selector:@selector(pollUpdates) userInfo:nil repeats:YES];
    [app run];
}

void MacUI::showWelcome() {
    window.contentViewController = welcomeVC;
    [window makeKeyAndOrderFront:nil];
}

void MacUI::showDeviceDiscovery() {
    window.contentViewController = discoveryVC;
    [window makeKeyAndOrderFront:nil];
}

void MacUI::showPairing(const std::string& deviceId) {
    NSString* nsDeviceId = [NSString stringWithUTF8String:deviceId.c_str()];
    pairingVC = [[PairingViewController alloc] initWithDeviceId:nsDeviceId ui:this];
    window.contentViewController = pairingVC;
    [window makeKeyAndOrderFront:nil];
}

void MacUI::showChat(const std::string& sessionId) {
    NSString* nsSessionId = [NSString stringWithUTF8String:sessionId.c_str()];
    chatVC = [[ChatViewController alloc] initWithSessionId:nsSessionId ui:this];
    window.contentViewController = chatVC;
    [window makeKeyAndOrderFront:nil];
}

void MacUI::showFileTransfer(const std::string& transferId) {
    NSString* nsTransferId = [NSString stringWithUTF8String:transferId.c_str()];
    transferVC = [[FileTransferViewController alloc] initWithTransferId:nsTransferId ui:this];
    window.contentViewController = transferVC;
    [window makeKeyAndOrderFront:nil];
}

void MacUI::showSettings() {
    window.contentViewController = settingsVC;
    [window makeKeyAndOrderFront:nil];
}

void MacUI::sendEvent(const std::string& eventJson) {
    if (core) {
        core_send_event(core, eventJson.c_str());
    }
}

void MacUI::pollUpdates() {
    if (!core) return;
    char* updateJson = core_recv_update(core);
    if (updateJson) {
        std::string json(updateJson);
        core_free_string(updateJson);
        // Parse and handle update
        // For example, if DeviceFound, add to discoveryVC
        if (json.find("DeviceFound") != std::string::npos) {
            // Extract id and name, simplistic
            size_t idStart = json.find("\"id\":\"") + 6;
            size_t idEnd = json.find("\"", idStart);
            std::string id = json.substr(idStart, idEnd - idStart);
            size_t nameStart = json.find("\"name\":\"") + 8;
            size_t nameEnd = json.find("\"", nameStart);
            std::string name = json.substr(nameStart, nameEnd - nameStart);
            NSString* nsId = [NSString stringWithUTF8String:id.c_str()];
            NSString* nsName = [NSString stringWithUTF8String:name.c_str()];
            [discoveryVC addDevice:nsId name:nsName];
        }
        // Handle other updates similarly
    }
}