#include "auto_update.h"
#include <iostream>
#include <thread>
#include <curl/curl.h>
#include <fstream>
#include <cstdlib>

#if defined(__APPLE__)
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <filesystem>
namespace fs = std::filesystem;
#elif defined(__linux__)
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <filesystem>
namespace fs = std::filesystem;
#endif

class AutoUpdate::Impl {
public:
    std::string current_version = "1.0.0";

    Impl() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~Impl() {
        curl_global_cleanup();
    }

    void check_for_updates(std::function<void(bool, const std::string&)> callback) {
        std::thread([this, callback]() {
            CURL* curl = curl_easy_init();
            if (curl) {
                std::string url = "https://api.github.com/repos/bluebeam/bluebeam/releases/latest";
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
                std::string response;
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                CURLcode res = curl_easy_perform(curl);
                curl_easy_cleanup(curl);

                if (res == CURLE_OK) {
                    // Parse JSON for version
                    std::string latest_version = "1.0.1"; // Mock
                    bool update = latest_version > current_version;
                    callback(update, latest_version);
                } else {
                    callback(false, "");
                }
            }
        }).detach();
    }

    void download_and_install(const std::string& url) {
        // Download and install
        std::cout << "Downloading update from " << url << std::endl;
        CURL* curl = curl_easy_init();
        if (curl) {
            std::string zip_path = get_temp_path() + "/update.zip";
            std::ofstream file(zip_path, std::ios::binary);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
            CURLcode res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            file.close();

            if (res == CURLE_OK) {
                // Extract and install
                std::cout << "Update downloaded, installing..." << std::endl;
                install_update(zip_path);
            }
        }
    }

    std::string get_temp_path() {
#if defined(__APPLE__) || defined(__linux__)
        return "/tmp";
#elif defined(_WIN32)
        char path[MAX_PATH];
        GetTempPathA(MAX_PATH, path);
        return std::string(path);
#endif
    }

    void install_update(const std::string& zip_path) {
        std::string extract_dir = get_temp_path() + "/bluebeam_update";
        // Create extract dir
        fs::create_directories(extract_dir);

        // Unzip
        std::string unzip_cmd = "unzip -q \"" + zip_path + "\" -d \"" + extract_dir + "\"";
        system(unzip_cmd.c_str());

        // Get current executable path
        std::string exe_path = get_executable_path();

        // Replace binary
#if defined(__APPLE__)
        // For macOS, assume update contains BlueBeam.app in extract_dir
        fs::path exe_fs_path(exe_path);
        fs::path app_bundle = exe_fs_path.parent_path().parent_path().parent_path(); // ../../../../BlueBeam.app
        fs::path update_app = fs::path(extract_dir) / "BlueBeam.app";
        if (fs::exists(update_app)) {
            fs::remove_all(app_bundle);
            fs::rename(update_app, app_bundle);
            // Restart app
            std::string restart_cmd = "open \"" + app_bundle.string() + "\"";
            system(restart_cmd.c_str());
            exit(0);
        }
#elif defined(_WIN32)
        // For Windows, assume update contains bluebeam.exe
        fs::path exe_dir = fs::path(exe_path).parent_path();
        fs::path update_exe = fs::path(extract_dir) / "bluebeam.exe";
        if (fs::exists(update_exe)) {
            fs::path backup = exe_path + ".bak";
            fs::rename(exe_path, backup);
            fs::rename(update_exe, exe_path);
            // Restart
            ShellExecuteA(NULL, "open", exe_path.c_str(), NULL, NULL, SW_SHOWNORMAL);
            exit(0);
        }
#elif defined(__linux__)
        // For Linux, assume update contains bluebeam binary
        fs::path exe_dir = fs::path(exe_path).parent_path();
        fs::path update_exe = fs::path(extract_dir) / "bluebeam";
        if (fs::exists(update_exe)) {
            fs::path backup = exe_path + ".bak";
            fs::rename(exe_path, backup);
            fs::rename(update_exe, exe_path);
            // Restart
            execl(exe_path.c_str(), exe_path.c_str(), NULL);
        }
#endif
    }

    std::string get_executable_path() {
#if defined(__APPLE__)
        char path[PATH_MAX];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            return std::string(path);
        }
        return "";
#elif defined(_WIN32)
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        return std::string(path);
#elif defined(__linux__)
        char path[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path)-1);
        if (len != -1) {
            path[len] = '\0';
            return std::string(path);
        }
        return "";
#endif
        return "";
    }

    void run_installer_hooks() {
        // Create necessary directories
        std::string app_data = get_app_data_path();
        mkdir(app_data.c_str(), 0755);
        // Other initial setup
        std::cout << "Installer hooks completed" << std::endl;
    }

    std::string get_app_data_path() {
#if defined(__APPLE__)
        return std::string(getenv("HOME")) + "/Library/Application Support/BlueBeam";
#elif defined(_WIN32)
        char path[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path) == S_OK) {
            return std::string(path) + "\\BlueBeam";
        }
        return "";
#elif defined(__linux__)
        const char* config_dir = getenv("XDG_DATA_HOME");
        if (config_dir) {
            return std::string(config_dir) + "/bluebeam";
        } else {
            return std::string(getenv("HOME")) + "/.local/share/bluebeam";
        }
#endif
        return "";
    }

private:
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* s) {
        size_t newLength = size * nmemb;
        s->append((char*)contents, newLength);
        return newLength;
    }
};

AutoUpdate::AutoUpdate() : pimpl(std::make_unique<Impl>()) {}
AutoUpdate::~AutoUpdate() = default;

void AutoUpdate::check_for_updates(std::function<void(bool update_available, const std::string& version)> callback) {
    pimpl->check_for_updates(callback);
}

void AutoUpdate::download_and_install(const std::string& url) {
    pimpl->download_and_install(url);
}

void AutoUpdate::run_installer_hooks() {
    pimpl->run_installer_hooks();
}