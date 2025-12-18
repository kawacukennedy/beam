#pragma once
#include <string>
#include <functional>

class Settings;

class AutoUpdate {
public:
    AutoUpdate(Settings* settings);
    ~AutoUpdate();

    void check_for_updates(std::function<void(bool update_available, const std::string& version)> callback);
    void download_and_install(const std::string& url);

    // Installer hook
    void run_installer_hooks();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};