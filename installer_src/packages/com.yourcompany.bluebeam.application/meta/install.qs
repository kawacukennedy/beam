var logFilePath = installer.value("TargetDir") + "/installer_log.txt";

// Function to append messages to the log file with levels
function logMessage(level, message) {
    var timestamp = QDateTime.currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    var formattedMessage = timestamp + " [" + level + "] - " + message + "\n";

    try {
        var file = new QFile(logFilePath);
        if (file.open(QIODevice.Append | QIODevice.Text)) {
            var stream = new QTextStream(file);
            stream << formattedMessage;
            file.close();
        }
    } catch (e) {
        print("ERROR: Could not write to log file: " + e + " - " + message);
    }
    print(formattedMessage); // Also print to console for immediate feedback
}

// Helper functions for different log levels
function logInfo(message) { logMessage("INFO", message); }
function logWarning(message) { logMessage("WARNING", message); }
function logError(message) { logMessage("ERROR", message); }
function logCritical(message) { logMessage("CRITICAL", message); }

// installscript.qs

function Controller() {
    // Constructor
    installer.installationFinished.connect(this, Controller.prototype.onInstallationFinished);

    // TODO: Implement window properties (width:640, height:480, resizable:false).
    // This typically involves setting properties on the main installer window object.
    // For example, in a custom Wizard.qml, or using specific API calls on the 'gui' object
    // if available (e.g., gui.setWindowFixedSize(640, 480)).
    // The 'resizable:false' might be set via gui.setWindowFlags() or similar.
}

Controller.prototype.PreInstallationChecksPageCallback = function() {
    var page = gui.pageById("PreInstallationChecksPage");
    if (page) {
        logInfo("Entering Pre-installation Checks Page.");

        // Check if a retry was requested
        if (installer.value("PreInstallationChecksRetry", false)) {
            logInfo("Retry requested for pre-installation checks.");
            installer.setValue("PreInstallationChecksRetry", false); // Reset the flag
        }

        // Initialize page UI
        page.statusText = "Starting pre-installation checks...";
        page.detailedMessages = "";
        page.progressValue = 0;
        page.progressMaximum = 4; // Number of checks
        page.showErrorModal = false; // Hide any previous error modal
        this.allChecksPassed = true; // Initialize flag for this run

        logInfo("Starting pre-installation checks.");

        var checks = [
            { name: "Disk Space (>=200MB)", func: this.performDiskSpaceCheck },
            { name: "Write Permissions", func: this.performWritePermissionCheck },
            { name: "Bluetooth Adapter", func: this.performBluetoothAdapterCheck },
            { name: "Dependencies", func: this.performDependencyCheck }
        ];

        var currentCheckIndex = 0;
        var controller = this; // Reference to the controller instance

        function runNextCheck() {
            if (currentCheckIndex < checks.length) {
                var check = checks[currentCheckIndex];
                page.statusText = "Running check: " + check.name + "...";
                page.detailedMessages += "\n" + check.name + "...";
                logInfo("Running check: " + check.name);

                // Perform the check
                var checkResult = check.func.call(controller, page); // Call with controller context

                if (checkResult.success) {
                    page.detailedMessages += "  [OK]";
                    currentCheckIndex++;
                    // Update progress bar
                    page.progressValue = (currentCheckIndex / checks.length) * page.progressMaximum;
                    runNextCheck();
                } else {
                    page.detailedMessages += "  [FAILED]";
                    page.errorMessage = "Failed: " + check.name + ". " + checkResult.message + " Please resolve the issue and retry.";
                    page.showErrorModal = true;
                    controller.allChecksPassed = false; // Mark overall checks as failed
                    logError("Check failed: " + check.name + ". Message: " + checkResult.message);
                }
            } else {
                page.statusText = "All pre-installation checks passed.";
                page.detailedMessages += "\nAll checks passed. Ready for installation.";
                page.complete = true;
                logInfo("All pre-installation checks passed.");
            }
        }

        runNextCheck();
    }
};

Controller.prototype.performDiskSpaceCheck = function(page) {
    logInfo("Performing disk space check.");
    var requiredSpaceMB = 200;
    var requiredSpaceBytes = requiredSpaceMB * 1024 * 1024;
    var freeSpaceBytes = installer.freeDiskSpace();

    if (freeSpaceBytes >= requiredSpaceBytes) {
        logInfo("Disk space check passed. Free space: " + (freeSpaceBytes / (1024 * 1024)).toFixed(2) + " MB.");
        return { success: true, message: "Available disk space: " + (freeSpaceBytes / (1024 * 1024)).toFixed(2) + "MB." };
    } else {
        var message = "Insufficient disk space. Required: " + requiredSpaceMB + " MB, Available: " + (freeSpaceBytes / (1024 * 1024)).toFixed(2) + " MB.";
        logError(message);
        return { success: false, message: message };
    }
};

Controller.prototype.performWritePermissionCheck = function(page) {
    logInfo("Performing write permission check.");
    var installPath = installer.value("TargetDir");
    if (installer.isWritable(installPath)) {
        logInfo("Write permission check passed for: " + installPath + ".");
        return { success: true, message: "Write permissions for installation path: OK." };
    } else {
        var message = "Write permissions denied for installation path: " + installPath + ".";
        logError(message);
        return { success: false, message: message };
    }
};

Controller.prototype.performBluetoothAdapterCheck = function(page) {
    logInfo("Performing Bluetooth adapter check (placeholder).");
    // TODO: Implement actual platform-specific Bluetooth adapter check.
    // For now, assuming success.
    logWarning("Bluetooth adapter check is a placeholder. Actual implementation needed for production.");
    return { success: true, message: "Bluetooth adapter check (placeholder) passed." };
};

Controller.prototype.performDependencyCheck = function(page) {
    logInfo("Performing dependency check.");
    var success = true;
    var message = "System dependencies: Met.";

    if (installer.isWin()) {
        // Windows: Check for VC++ Redistributable
        // This is a simplified check. A robust check would involve checking specific registry keys
        // or looking for specific DLLs in System32.
        // Example: Check for a common VC++ Redistributable 2015-2019 (version 14.x)
        var vcredistPresent = installer.execute("reg", ["query", "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64", "/v", "Installed"]).length > 0;
        if (!vcredistPresent) {
            success = false;
            message = "VC++ Redistributable 2015-2019 (x64) is not installed. Please install it from Microsoft's website.";
            logError(message);
        } else {
            logInfo("VC++ Redistributable 2015-2019 (x64) found.");
        }
    } else if (installer.isMac()) {
        // macOS: Verify Qt dylibs (if not bundled), other system libraries.
        // Assuming Qt dylibs are bundled with the application for simplicity.
        // For other system libraries, you might check for their presence in /usr/lib or /System/Library/Frameworks.
        logWarning("macOS dependency check for Qt dylibs is a placeholder. Assuming bundled.");
        // Example: Check for a specific framework
        // var coreBluetoothFramework = installer.fileExists("/System/Library/Frameworks/CoreBluetooth.framework");
        // if (!coreBluetoothFramework) {
        //     success = false;
        //     message = "CoreBluetooth.framework is missing.";
        //     logError(message);
        // }
    } else if (installer.isLinux()) {
        // Linux: Check for bluez and dbus packages
        var bluezPresent = false;
        var dbusPresent = false;

        if (installer.execute("which", ["dpkg"]).length > 0) { // Debian/Ubuntu
            bluezPresent = installer.execute("dpkg", ["-s", "bluez"]).length > 0;
            dbusPresent = installer.execute("dpkg", ["-s", "dbus"]).length > 0;
        } else if (installer.execute("which", ["rpm"]).length > 0) { // Fedora/CentOS
            bluezPresent = installer.execute("rpm", ["-q", "bluez"]).length > 0;
            dbusPresent = installer.execute("rpm", ["-q", "dbus"]).length > 0;
        } else {
            logWarning("No known package manager (dpkg, rpm) found for Linux dependency check.");
            // Fallback or assume success if package manager not found
            success = true;
        }

        if (!bluezPresent) {
            success = false;
            message = "BlueZ package is not installed. Please install it using your system's package manager.";
            logError(message);
        } else if (!dbusPresent) {
            success = false;
            message = "D-Bus package is not installed. Please install it using your system's package manager.";
            logError(message);
        } else {
            logInfo("BlueZ and D-Bus packages found.");
        }
    }

    if (success) {
        logInfo("Dependency check passed.");
        return { success: true, message: message };
    } else {
        logError("Dependency check failed: " + message);
        return { success: false, message: message };
    }
};

Controller.prototype.InstallationDirectoryPageCallback = function() {
    var page = gui.pageById("InstallationDirectoryPage");
    if (page) {
        logInfo("Entering Installation Directory Page.");
        // Update disk space label
        this.updateDiskSpaceLabel(page);

        // Connect text changed signal for validation
        page.pathTextBox.textChanged.connect(this, function() {
            controller.validateInstallationPath(page);
        });

        // Initial validation
        this.validateInstallationPath(page);
    }
};

Controller.prototype.updateDiskSpaceLabel = function(page) {
    logInfo("Updating disk space label.");
    // Placeholder for actual disk space calculation
    // This would involve platform-specific calls or a Qt API if available
    // For now, a dummy value
    page.diskSpaceText = "Available: 10 GB (Dummy)";
    logWarning("Disk space calculation is a dummy value. Actual implementation needed.");
};

Controller.prototype.validateInstallationPath = function(page) {
    logInfo("Validating installation path: " + page.installationPath);
    var path = page.installationPath;
    var isValid = true;
    var errorMessage = "";
    var requiredSpaceBytes = 200 * 1024 * 1024; // 200MB

    if (path === "") {
        isValid = false;
        errorMessage = "Installation path cannot be empty.";
        logWarning("Validation failed: Installation path is empty.");
    } else {
        // Check writability
        if (!installer.isWritable(path)) {
            isValid = false;
            errorMessage = "Directory is not writable.";
            logError("Validation failed: Directory is not writable: " + path);
        }

        // Check sufficient disk space
        if (isValid) { // Only check disk space if writable
            var freeSpace = installer.freeDiskSpace(path);
            if (freeSpace < requiredSpaceBytes) {
                isValid = false;
                errorMessage = "Insufficient disk space (requires 200MB). Available: " + (freeSpace / (1024 * 1024)).toFixed(2) + " MB.";
                logError("Validation failed: Insufficient disk space. Available: " + (freeSpace / (1024 * 1024)).toFixed(2) + " MB.");
            } else {
                logInfo("Disk space sufficient. Available: " + (freeSpace / (1024 * 1024)).toFixed(2) + " MB.");
            }
        }
    }

    page.validationErrorText = errorMessage;
    page.complete = isValid; // Enable/disable Next button
    if (isValid) {
        logInfo("Installation path '" + path + "' is valid.");
    } else {
        logWarning("Installation path '" + path + "' is invalid. Error: " + errorMessage);
    }
};

Controller.prototype.InstallationPageCallback = function() {
    var page = gui.pageById("InstallationProgressPage"); // Reference to the custom QML page
    if (page) {
        logInfo("Entering Installation Page.");
        // Start time for elapsed time calculation
        this.installationStartTime = new Date();

        // Connect to installer signals for logging and error reporting to QML page
        installer.progressChanged.connect(this, function() {
            var progress = installer.progress();
            logInfo("Installer progress: " + progress + "%");
            // Update QML page's progress bar and percentage label
            page.currentProgress = progress;
            page.logWindow.append(qsTr("Overall progress: %1%").arg(progress));
        });
        installer.statusChanged.connect(this, function() {
            var status = installer.status();
            logInfo("Installer status: " + status);
            // Update QML page's log window with status messages
            page.logWindow.append(status);
        });
        installer.error.connect(this, function(message) {
            logError("Installation error: " + message);
            // Trigger the custom error modal on the QML page
            page.errorMessage = message;
            page.showErrorModal = true;
            page.installationRunning = false; // Stop QML page's simulation/progress
            controller.sendTelemetry("failure", message); // Send telemetry on failure
        });

        logInfo("Installation started at: " + this.installationStartTime);

        // Disable the QML page's internal simulation as we're now using actual installer progress
        page.installationRunning = false; // Ensure QML simulation is off
        // The QML page's progress bar will now be driven by installer.progressChanged
    }
};

Controller.prototype.FinishPageCallback = function() {
    var page = gui.pageById("FinishPage");
    if (page) {
        logInfo("Entering Finish Page.");
        // Customize the finish page
        // Launch App checkbox
        page.runProgramCheckBox.checked = installer.value("LaunchApp", true); // Default to true
        page.runProgramCheckBox.text = "Launch App";

        // Add Release Notes link
        var releaseNotesLink = page.createWidget("Label");
        releaseNotesLink.setText("<a href=\"https://yourcompany.com/releases\">Release Notes</a>");
        releaseNotesLink.setOpenExternalLinks(true);
        page.insertWidget(page.runProgramCheckBox, releaseNotesLink); // Insert after checkbox

        // Add Support link
        var supportLink = page.createWidget("Label");
        supportLink.setText("<a href=\"https://yourcompany.com/support\">Support</a>");
        supportLink.setOpenExternalLinks(true);
        page.insertWidget(releaseNotesLink, supportLink); // Insert after release notes

        // TODO: Implement "confetti fade 1s" animation for the "Finish" button.
        // This would likely require creating a custom QML page for the finish screen
        // to have full control over UI elements and apply QML animations.
    }
};


Controller.prototype.onInstallationFinished = function() {
    // Clean up or final actions after installation

    var targetDir = installer.value("TargetDir");
    var appName = "BlueConnect Desktop"; // Updated from config.xml
    var appExecutable = targetDir + "/BlueConnectDesktop"; // Assuming this is the main executable

    // Create Start Menu Shortcut
    if (installer.value("StartMenuShortcut", false)) {
        logInfo("Starting creation of Start Menu shortcut for " + appName);
        installer.createShortcut(appExecutable, appName, "StartMenu");
        logInfo("Created Start Menu shortcut for " + appName);
    }

    // Create Desktop Shortcut
    if (installer.value("DesktopShortcut", false)) {
        logInfo("Starting creation of Desktop shortcut for " + appName);
        installer.createShortcut(appExecutable, appName, "Desktop");
        logInfo("Created Desktop shortcut for " + appName);
    }

    // Implement "Auto-start Bluetooth service" logic.
    if (installer.value("AutoStartService", false)) {
        logInfo("Attempting to configure auto-start Bluetooth service for " + appName);
        if (installer.isWin()) {
            this.configureWindowsService(appExecutable, appName);
        } else if (installer.isMac()) {
            this.configureMacLaunchAgent(appExecutable, appName);
        } else if (installer.isLinux()) {
            this.configureLinuxSystemdService(appExecutable, appName);
        }
        logInfo("Finished configuring auto-start Bluetooth service for " + appName);
    }

    // Send telemetry for successful installation
    this.sendTelemetry("success");
    logInfo("Installation finished actions completed.");
};

// Function to send telemetry data
Controller.prototype.sendTelemetry = function(status, errorMessage) {
    var telemetryData = {
        status: status,
        os: installer.os(),
        arch: installer.arch(),
        version: installer.productVersion(),
        installationDuration: (new Date().getTime() - this.installationStartTime.getTime()) / 1000, // in seconds
        optionalFeatures: {
            autoStartService: installer.value("AutoStartService", false),
            startMenuShortcut: installer.value("StartMenuShortcut", false),
            desktopShortcut: installer.value("DesktopShortcut", false)
        },
        errorMessage: errorMessage || ""
    };
    logInfo("Telemetry Data: " + JSON.stringify(telemetryData));
    // In a real scenario, this data would be sent to an analytics endpoint.
    // Example: installer.network.post("https://yourcompany.com/analytics", JSON.stringify(telemetryData));
};

// Placeholder functions for platform-specific service configuration
Controller.prototype.configureWindowsService = function(appExecutable, appName) {
    logInfo("Configuring Windows Service for " + appName);
    var serviceName = appName.replace(/\s/g, '') + "Service"; // e.g., BlueConnectDesktopService
    var serviceDisplayName = appName + " Service";
    var serviceDescription = "Provides Bluetooth connectivity for " + appName;

    // Create the service
    logInfo("Executing sc create for Windows Service '" + serviceName + "'.");
    var createCommand = [
        "sc", "create", serviceName,
        "binPath=\"" + appExecutable + " --service\"",
        "DisplayName=\"" + serviceDisplayName + "\"",
        "start=\"auto\"",
        "obj=\"LocalSystem\"",
        "type=\"own\""
    ];
    var createResult = installer.execute("cmd", ["/C", createCommand.join(" ")]);
    if (createResult === 0) {
        logInfo("Windows Service '" + serviceName + "' created successfully.");
        // Set service description
        installer.execute("sc", ["description", serviceName, serviceDescription]);

        // Start the service
        logInfo("Executing sc start for Windows Service '" + serviceName + "'.");
        var startResult = installer.execute("sc", ["start", serviceName]);
        if (startResult === 0) {
            logInfo("Windows Service '" + serviceName + "' started successfully.");
        } else {
            logError("Failed to start Windows Service '" + serviceName + "'. Exit code: " + startResult);
        }
    } else {
        logError("Failed to create Windows Service '" + serviceName + "'. Exit code: " + createResult);
    }
};

Controller.prototype.configureMacLaunchAgent = function(appExecutable, appName) {
    logInfo("Configuring macOS Launch Agent for " + appName);
    var domain = "com.yourcompany.blueconnect"; // Use the bundle ID as a base
    var label = domain + ".bluetoothservice";
    var plistFileName = label + ".plist";
    var launchAgentsDir = installer.environmentVariable("HOME") + "/Library/LaunchAgents";
    var plistPath = launchAgentsDir + "/" + plistFileName;

    // Ensure the LaunchAgents directory exists
    installer.makePath(launchAgentsDir);

    // Create the .plist content
    var plistContent = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    plistContent += "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    plistContent += "<plist version=\"1.0\">\n";
    plistContent += "<dict>\n";
    plistContent += "    <key>Label</key>\n";
    plistContent += "    <string>" + label + "</string>\n";
    plistContent += "    <key>ProgramArguments</key>\n";
    plistContent += "    <array>\n";
    plistContent += "        <string>" + appExecutable + "</string>\n";
    plistContent += "        <string>--service</string>\n"; // Assuming the app can run in service mode
    plistContent += "    </array>\n";
    plistContent += "    <key>RunAtLoad</key>\n";
    plistContent += "    <true/>\n";
    plistContent += "    <key>KeepAlive</key>\n";
    plistContent += "    <true/>\n";
    plistContent += "    <key>StandardOutPath</key>\n";
    plistContent += "    <string>" + installer.value("TargetDir") + "/" + appName.replace(/\s/g, '') + "_launchagent.log</string>\n";
    plistContent += "    <key>StandardErrorPath</key>\n";
    plistContent += "    <string>" + installer.value("TargetDir") + "/" + appName.replace(/\s/g, '') + "_launchagent_error.log</string>\n";
    plistContent += "</dict>\n";
    plistContent += "</plist>\n";

    // Write the .plist file
    installer.writeFile(plistPath, plistContent);
    logInfo("macOS Launch Agent .plist written to: " + plistPath);

    // Load the Launch Agent
    logInfo("Executing launchctl load for macOS Launch Agent '" + label + "'.");
    var loadResult = installer.execute("launchctl", ["load", "-w", plistPath]);
    if (loadResult === 0) {
        logInfo("macOS Launch Agent '" + label + "' loaded successfully.");
    } else {
        logError("Failed to load macOS Launch Agent '" + label + "'. Exit code: " + loadResult);
    }
};

Controller.prototype.configureLinuxSystemdService = function(appExecutable, appName) {
    logInfo("Configuring Linux systemd service for " + appName);
    var serviceName = appName.toLowerCase().replace(/\s/g, '-') + ".service"; // e.g., blueconnect-desktop.service
    var servicePath = "/etc/systemd/system/" + serviceName;

    // Create the .service file content
    var serviceContent = "[Unit]\n";
    serviceContent += "Description=" + appName + " Bluetooth Service\n";
    serviceContent += "After=network.target\n\n";
    serviceContent += "[Service]\n";
    serviceContent += "ExecStart=" + appExecutable + " --service\n"; // Assuming the app can run in service mode
    serviceContent += "Restart=on-failure\n";
    serviceContent += "User=root\n"; // Or a less privileged user if applicable
    serviceContent += "Group=root\n\n";
    serviceContent += "[Install]\n";
    serviceContent += "WantedBy=multi-user.target\n";

    // Write the .service file
    installer.writeFile(servicePath, serviceContent);
    logInfo("Linux systemd .service file written to: " + servicePath);

    // Reload systemd daemon
    logInfo("Executing systemctl daemon-reload.");
    var daemonReloadResult = installer.execute("systemctl", ["daemon-reload"]);
    if (daemonReloadResult !== 0) {
        logError("Failed to reload systemd daemon. Exit code: " + daemonReloadResult);
    }

    // Enable the service
    logInfo("Executing systemctl enable for Linux systemd service '" + serviceName + "'.");
    var enableResult = installer.execute("systemctl", ["enable", serviceName]);
    if (enableResult === 0) {
        logInfo("Linux systemd service '" + serviceName + "' enabled successfully.");
        // Start the service
        logInfo("Executing systemctl start for Linux systemd service '" + serviceName + "'.");
        var startResult = installer.execute("systemctl", ["start", serviceName]);
        if (startResult === 0) {
            logInfo("Linux systemd service '" + serviceName + "' started successfully.");
        } else {
            logError("Failed to start Linux systemd service '" + serviceName + "'. Exit code: " + startResult);
        }
    } else {
        logError("Failed to enable Linux systemd service '" + serviceName + "'. Exit code: " + enableResult);
    }
};
// Create an instance of the controller
var controller = new Controller();

// Connect the PreInstallationChecksPage's entered signal to the callback
gui.pageById("PreInstallationChecksPage").entered.connect(controller, "PreInstallationChecksPageCallback");

// Connect the InstallationDirectoryPage's entered signal to the callback
gui.pageById("InstallationDirectoryPage").entered.connect(controller, "InstallationDirectoryPageCallback");

// Connect the InstallationPage's entered signal to the callback
gui.pageById("InstallationPage").entered.connect(controller, "InstallationPageCallback");

// Connect the FinishPage's entered signal to the callback
gui.pageById("FinishPage").entered.connect(controller, "FinishPageCallback");