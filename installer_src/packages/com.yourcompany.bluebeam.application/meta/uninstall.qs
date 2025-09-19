function Controller() {
    installer.uninstallationFinished.connect(this, Controller.prototype.onUninstallationFinished);
}

Controller.prototype.onUninstallationFinished = function() {
    logInfo("Starting uninstallation finished actions.");
    var targetDir = installer.value("TargetDir");
    var appName = "BlueConnect Desktop";
    var appExecutable = targetDir + "/BlueConnectDesktop";

    // Remove Start Menu Shortcut
    if (installer.value("StartMenuShortcut", false)) {
        installer.removeShortcut(appExecutable, appName, "StartMenu");
        logInfo("Removed Start Menu shortcut for " + appName);
    }

    // Remove Desktop Shortcut
    if (installer.value("DesktopShortcut", false)) {
        installer.removeShortcut(appExecutable, appName, "Desktop");
        logInfo("Removed Desktop shortcut for " + appName);
    }

    // Unimplement "Auto-start Bluetooth service" logic.
    if (installer.value("AutoStartService", false)) {
        logInfo("Attempting to unconfigure auto-start Bluetooth service for " + appName);
        if (installer.isWin()) {
            this.unconfigureWindowsService(appExecutable, appName);
        } else if (installer.isMac()) {
            this.unconfigureMacLaunchAgent(appExecutable, appName);
        } else if (installer.isLinux()) {
            this.unconfigureLinuxSystemdService(appExecutable, appName);
        }
    }
    // Remove installer log file
    var logFilePath = installer.value("TargetDir") + "/installer_log.txt";
    if (installer.fileExists(logFilePath)) {
        installer.removeFile(logFilePath);
        logInfo("Removed installer log file: " + logFilePath);
    }
    logInfo("Uninstallation finished actions completed.");
};

// Placeholder functions for platform-specific service unconfiguration
Controller.prototype.unconfigureWindowsService = function(appExecutable, appName) {
    logInfo("Unconfiguring Windows Service for " + appName);
    var serviceName = appName.replace(/\s/g, '') + "Service";

    // Stop the service first
    var stopResult = installer.execute("sc", ["stop", serviceName]);
    if (stopResult === 0 || stopResult === 1062) { // 1062 means service not running
        logInfo("Windows Service '" + serviceName + "' stopped or was not running.");
        // Delete the service
        var deleteResult = installer.execute("sc", ["delete", serviceName]);
        if (deleteResult === 0) {
            logInfo("Windows Service '" + serviceName + "' deleted successfully.");
        } else {
            logError("Failed to delete Windows Service '" + serviceName + "'. Exit code: " + deleteResult);
        }
    } else {
        logError("Failed to stop Windows Service '" + serviceName + "'. Exit code: " + stopResult);
    }
};

Controller.prototype.unconfigureMacLaunchAgent = function(appExecutable, appName) {
    logInfo("Unconfiguring macOS Launch Agent for " + appName);
    var domain = "com.yourcompany.blueconnect";
    var label = domain + ".bluetoothservice";
    var plistFileName = label + ".plist";
    var launchAgentsDir = installer.environmentVariable("HOME") + "/Library/LaunchAgents";
    var plistPath = launchAgentsDir + "/" + plistFileName;

    // Unload the Launch Agent
    var unloadResult = installer.execute("launchctl", ["unload", plistPath]);
    if (unloadResult === 0) {
        logInfo("macOS Launch Agent '" + label + "' unloaded successfully.");
    } else {
        logError("Failed to unload macOS Launch Agent '" + label + "'. Exit code: " + unloadResult);
    }

    // Delete the .plist file
    if (installer.fileExists(plistPath)) {
        installer.removeFile(plistPath);
        logInfo("macOS Launch Agent .plist file deleted: " + plistPath);
    } else {
        logWarning("macOS Launch Agent .plist file not found: " + plistPath);
    }
};

Controller.prototype.unconfigureLinuxSystemdService = function(appExecutable, appName) {
    logInfo("Unconfiguring Linux systemd service for " + appName);
    var serviceName = appName.toLowerCase().replace(/\s/g, '-') + ".service";
    var servicePath = "/etc/systemd/system/" + serviceName;

    // Stop the service
    var stopResult = installer.execute("systemctl", ["stop", serviceName]);
    if (stopResult !== 0) {
        logWarning("Failed to stop Linux systemd service '" + serviceName + "'. Exit code: " + stopResult);
    }

    // Disable the service
    var disableResult = installer.execute("systemctl", ["disable", serviceName]);
    if (disableResult !== 0) {
        logWarning("Failed to disable Linux systemd service '" + serviceName + "'. Exit code: " + disableResult);
    }

    // Delete the .service file
    if (installer.fileExists(servicePath)) {
        installer.removeFile(servicePath);
        logInfo("Linux systemd .service file deleted: " + servicePath);
    } else {
        logWarning("Linux systemd .service file not found: " + servicePath);
    }

    // Reload systemd daemon
    var daemonReloadResult = installer.execute("systemctl", ["daemon-reload"]);
    if (daemonReloadResult !== 0) {
        logError("Failed to reload systemd daemon. Exit code: " + daemonReloadResult);
    }
};

// Create an instance of the controller
new Controller();
