function Component() {
    // Constructor
}

Component.prototype.createOperations = function() {
    // Call the base createOperations
    component.createOperations();

    // -- Shortcut and Menu Creation --
    if (installer.value("os") === "win") {
        // Windows specific operations
        if (component.userInterface() && component.userInterface().componentSelectionPage().componentIsSelected(component.name())) {
            component.addOperation("CreateShortcut", "@TargetDir@/bin/BlueConnect.exe", "@StartMenuDir@/BlueConnect.lnk", "workingDirectory=@TargetDir@/bin");
            component.addOperation("CreateShortcut", "@TargetDir@/bin/BlueConnect.exe", "@DesktopDir@/BlueConnect.lnk", "workingDirectory=@TargetDir@/bin");
        }
    } else if (installer.value("os") === "mac") {
        // macOS specific operations
        // On macOS, applications are typically just moved to the /Applications folder.
        // The createOperations for the data files will handle this.
        // We can add a symlink to the desktop if requested.
        if (component.userInterface() && component.userInterface().componentSelectionPage().componentIsSelected("DesktopShortcut")) {
            component.addOperation("CreateSymlink", "@TargetDir@/BlueConnect.app", "@DesktopDir@/BlueConnect");
        }
    } else if (installer.value("os") === "x11") {
        // Linux specific operations
        if (component.userInterface() && component.userInterface().componentSelectionPage().componentIsSelected("DesktopShortcut")) {
            var content = "[Desktop Entry]\nType=Application\nName=BlueConnect\nExec=@TargetDir@/bin/BlueConnect\nIcon=@TargetDir@/bin/icon.png\nTerminal=false";
            component.addOperation("CreateDesktopEntry", "/usr/share/applications/blueconnect.desktop", content);
        }
    }
}