#pragma once
#include <string>
#include <vector>
#include <memory>

class Settings {
public:
    Settings();
    ~Settings();

    // Profile settings
    void set_user_name(const std::string& name);
    std::string get_user_name();

    // Preferences
    void set_theme(const std::string& theme); // "light", "dark", "high_contrast"
    std::string get_theme();

    void set_download_path(const std::string& path);
    std::string get_download_path();

    // Security
    void set_encryption_enabled(bool enabled);
    bool get_encryption_enabled();

    // Trusted devices
    void add_trusted_device(const std::string& device_id);
    void remove_trusted_device(const std::string& device_id);
    std::vector<std::string> get_trusted_devices();

    // App settings
    void set_first_run(bool first);
    bool is_first_run();

    void save();
    void load();

    // Filesystem paths
    std::string get_app_data_path();
    std::string get_temp_path();
    std::string get_documents_path();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};