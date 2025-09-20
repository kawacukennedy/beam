/*
 * Global control script for the BlueConnect Desktop installer.
 */

function Controller() {
    installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
    installer.setDefaultPageVisible(QInstaller.ReadyForInstallation, false);
}

Controller.prototype.runPreInstallChecks = function() {
    var page = gui.pageById(QInstaller.PreInstallationChecksPage);

    // 1. Disk Space Check
    var diskSpaceNeeded = 200 * 1024 * 1024; // 200 MB
    var freeDiskSpace = installer.freeDiskSpace(installer.value("TargetDir"));
    var diskSpaceOK = freeDiskSpace > diskSpaceNeeded;
    installer.setValue("CheckDiskSpaceResult", diskSpaceOK ? 1 : 0);
    page.findChild("diskSpaceCheck").status = diskSpaceOK ? 1 : 0;

    // 2. Write Permission Check
    var writePermissionOK = installer.isWritable(installer.value("TargetDir"));
    installer.setValue("CheckWritePermissionResult", writePermissionOK ? 1 : 0);
    page.findChild("writePermCheck").status = writePermissionOK ? 1 : 0;

    // 3. Bluetooth Adapter Check (simulated)
    // In a real scenario, this would involve platform-specific code execution.
    var bluetoothOK = true; // Assume present for now
    installer.setValue("CheckBluetoothResult", bluetoothOK ? 1 : 0);
    page.findChild("bluetoothCheck").status = bluetoothOK ? 1 : 0;

    // 4. Dependencies Check (simulated)
    var depsOK = true; // Assume present for now
    installer.setValue("CheckDependenciesResult", depsOK ? 1 : 0);
    page.findChild("depsCheck").status = depsOK ? 1 : 0;

    var allChecksPassed = diskSpaceOK && writePermissionOK && bluetoothOK && depsOK;
    installer.setValue("PreInstallChecksPassed", allChecksPassed);
    return allChecksPassed;
}
    // -- Installer-wide state --
    installer.setValue("EulaScrolledToEnd", false);
    installer.setValue("EulaAccepted", false);
    installer.setValue("PreInstallChecksPassed", false);

    // -- Pre-installation Checks --
    // These values will be set by the PreInstallationChecksPage
    installer.setValue("CheckDiskSpaceResult", -1); // -1: Not run, 0: Fail, 1: Pass
    installer.setValue("CheckWritePermissionResult", -1);
    installer.setValue("CheckBluetoothResult", -1);
    installer.setValue("CheckDependenciesResult", -1);
}

Controller.prototype.WelcomePageCallback = function() {
    var page = gui.pageById(QInstaller.WelcomePage);
    // In the JSON, next is disabled until EULA is scrolled.
    // This implies a dependency between Welcome and License pages, which is unusual.
    // A better UX is to enable Next on Welcome page always, and handle EULA logic on License page.
    // For now, we will stick to the logic of the QML file.
}

Controller.prototype.LicenseAgreementPageCallback = function() {
    var page = gui.pageById(QInstaller.LicenseAgreementPage);
    var nextButton = gui.findChild(page, "nextButton");

    // Connect to signals from QML
    page.eulaScrolledToEndChanged.connect(function(scrolled) {
        installer.setValue("EulaScrolledToEnd", scrolled);
        updateLicenseNextButtonState();
    });

    var checkbox = gui.findChild(page, "licenseCheckbox");
    checkbox.checkedChanged.connect(function() {
        installer.setValue("EulaAccepted", checkbox.checked);
        updateLicenseNextButtonState();
    });

    function updateLicenseNextButtonState() {
        var accepted = installer.value("EulaAccepted");
        var scrolled = installer.value("EulaScrolledToEnd");
        nextButton.enabled = accepted && scrolled;
    }

    // Set initial state
    updateLicenseNextButtonState();
}

Controller.prototype.PreInstallationChecksPageCallback = function() {
    var page = gui.pageById(QInstaller.PreInstallationChecksPage);
    var nextButton = gui.findChild(page, "nextButton");
    nextButton.enabled = false; // Disable until checks pass

    // This is where we would trigger the checks defined in the QML page.
    // The actual checks would be implemented here or in a helper script.
    // For now, we simulate the checks.

    // TODO: Implement actual checks
    installer.setValue("CheckDiskSpaceResult", 1);
    installer.setValue("CheckWritePermissionResult", 1);
    installer.setValue("CheckBluetoothResult", 1);
    installer.setValue("CheckDependenciesResult", 1);

    var allChecksPassed = installer.value("CheckDiskSpaceResult") === 1 &&
                          installer.value("CheckWritePermissionResult") === 1 &&
                          installer.value("CheckBluetoothResult") === 1 &&
                          installer.value("CheckDependenciesResult") === 1;

    installer.setValue("PreInstallChecksPassed", allChecksPassed);
    nextButton.enabled = allChecksPassed;
}

Controller.prototype.TargetDirectoryPageCallback = function() {
    var page = gui.pageById(QInstaller.TargetDirectoryPage);
    var nextButton = gui.findChild(page, "nextButton");
    var targetDir = gui.findChild(page, "TargetDirectoryLineEdit");

    // Add validation logic
    targetDir.textChanged.connect(function() {
        var dir = targetDir.text;
        // Basic check, more robust checks for writability and space are needed
        if (dir.length > 0) {
            installer.setInstallerBaseBinary("CreateShortcut", dir + "/app.exe");
            nextButton.enabled = true;
        } else {
            nextButton.enabled = false;
        }
    });
}

Controller.prototype.ComponentSelectionPageCallback = function() {
    var page = gui.pageById(QInstaller.ComponentSelectionPage);
    // Logic for optional features can be handled here if needed
}

Controller.prototype.FinishedPageCallback = function() {
    var page = gui.pageById(QInstaller.FinishedPage);
    var launchCheckBox = gui.findChild(page, "LaunchApplicationCheckBox");
    if (launchCheckBox && launchCheckBox.checked) {
        installer.execute(installer.value("TargetDir") + "/bin/BlueConnect");
    }
}
