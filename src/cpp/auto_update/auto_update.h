#pragma once
#include <string>
#include <functional>

class AutoUpdate {
public:
    AutoUpdate();
    ~AutoUpdate();

    void check_for_updates(std::function<void(bool update_available, const std::string& version)> callback);
    void download_and_install(const std::string& url);

    // Installer hook
    void run_installer_hooks();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};