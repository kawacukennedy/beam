#include "settings.h"
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#elif defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#include <dpapi.h>
#elif defined(__linux__)
#include <glib.h>
#include <glib/gkeyfile.h>
#endif

class Settings::Impl {
public:
    std::map<std::string, std::string> string_settings;
    std::map<std::string, bool> bool_settings;
    std::vector<std::string> trusted_devices;

    Impl() {
        load();
    }

    ~Impl() {
        save();
    }

    void save() {
#if defined(__APPLE__)
        CFStringRef appID = CFSTR("com.bluebeam.app");
        CFPreferencesSetAppValue(CFSTR("user_name"), CFStringCreateWithCString(NULL, string_settings["user_name"].c_str(), kCFStringEncodingUTF8), appID);
        CFPreferencesSetAppValue(CFSTR("theme"), CFStringCreateWithCString(NULL, string_settings["theme"].c_str(), kCFStringEncodingUTF8), appID);
        CFPreferencesSetAppValue(CFSTR("download_path"), CFStringCreateWithCString(NULL, string_settings["download_path"].c_str(), kCFStringEncodingUTF8), appID);
        CFPreferencesSetAppValue(CFSTR("encryption_enabled"), bool_settings["encryption_enabled"] ? kCFBooleanTrue : kCFBooleanFalse, appID);
        CFPreferencesSetAppValue(CFSTR("first_run"), bool_settings["first_run"] ? kCFBooleanTrue : kCFBooleanFalse, appID);
        CFPreferencesAppSynchronize(appID);
        // Save trusted devices to Keychain
        for (size_t i = 0; i < trusted_devices.size(); ++i) {
            CFStringRef key = CFStringCreateWithFormat(NULL, NULL, CFSTR("trusted_device_%zu"), i);
            CFStringRef value = CFStringCreateWithCString(NULL, trusted_devices[i].c_str(), kCFStringEncodingUTF8);
            SecItemDelete((CFDictionaryRef)CFDictionaryCreate(NULL, (const void*[]){kSecClass, kSecAttrService, kSecAttrAccount}, (const void*[]){kSecClassGenericPassword, appID, key}, 3, NULL, NULL));
            CFDictionaryRef query = CFDictionaryCreate(NULL, (const void*[]){kSecClass, kSecAttrService, kSecAttrAccount, kSecValueData}, (const void*[]){kSecClassGenericPassword, appID, key, value}, 4, NULL, NULL);
            SecItemAdd(query, NULL);
            CFRelease(key);
            CFRelease(value);
            CFRelease(query);
        }
#elif defined(_WIN32)
        HKEY hKey;
        RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\BlueBeam", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
        RegSetValueEx(hKey, "user_name", 0, REG_SZ, (BYTE*)string_settings["user_name"].c_str(), string_settings["user_name"].size() + 1);
        RegSetValueEx(hKey, "theme", 0, REG_SZ, (BYTE*)string_settings["theme"].c_str(), string_settings["theme"].size() + 1);
        RegSetValueEx(hKey, "download_path", 0, REG_SZ, (BYTE*)string_settings["download_path"].c_str(), string_settings["download_path"].size() + 1);
        DWORD enc = bool_settings["encryption_enabled"] ? 1 : 0;
        RegSetValueEx(hKey, "encryption_enabled", 0, REG_DWORD, (BYTE*)&enc, sizeof(DWORD));
        DWORD first = bool_settings["first_run"] ? 1 : 0;
        RegSetValueEx(hKey, "first_run", 0, REG_DWORD, (BYTE*)&first, sizeof(DWORD));
        RegCloseKey(hKey);
        // Save trusted devices encrypted
        std::string devices_str;
        for (const auto& d : trusted_devices) devices_str += d + ";";
        DATA_BLOB dataIn = { (DWORD)devices_str.size(), (BYTE*)devices_str.data() };
        DATA_BLOB dataOut = {0};
        if (CryptProtectData(&dataIn, L"BlueBeamTrustedDevices", NULL, NULL, NULL, CRYPTPROTECT_LOCAL_MACHINE, &dataOut)) {
            RegSetValueEx(hKey, "trusted_devices", 0, REG_BINARY, dataOut.pbData, dataOut.cbData);
            LocalFree(dataOut.pbData);
        }
#elif defined(__linux__)
        GKeyFile* keyfile = g_key_file_new();
        g_key_file_set_string(keyfile, "profile", "user_name", string_settings["user_name"].c_str());
        g_key_file_set_string(keyfile, "preferences", "theme", string_settings["theme"].c_str());
        g_key_file_set_string(keyfile, "preferences", "download_path", string_settings["download_path"].c_str());
        g_key_file_set_boolean(keyfile, "security", "encryption_enabled", bool_settings["encryption_enabled"]);
        g_key_file_set_boolean(keyfile, "app", "first_run", bool_settings["first_run"]);

        gchar* data = g_key_file_to_data(keyfile, NULL, NULL);
        std::ofstream file(g_get_user_config_dir() + std::string("/bluebeam/settings.ini"));
        file << data;
        file.close();
        g_free(data);
        g_key_file_free(keyfile);
#endif
    }

    void load() {
#if defined(__APPLE__)
        CFStringRef appID = CFSTR("com.bluebeam.app");
        CFStringRef value;
        value = (CFStringRef)CFPreferencesCopyAppValue(CFSTR("user_name"), appID);
        if (value) {
            char buffer[256];
            CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8);
            string_settings["user_name"] = buffer;
            CFRelease(value);
        }
        // Similar for others
        bool_settings["first_run"] = true; // Default
        // Load trusted devices from Keychain
#elif defined(_WIN32)
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\BlueBeam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char buffer[256];
            DWORD size = sizeof(buffer);
            RegQueryValueEx(hKey, "user_name", NULL, NULL, (BYTE*)buffer, &size);
            string_settings["user_name"] = buffer;
            // Similar for others
            DWORD enc;
            RegQueryValueEx(hKey, "encryption_enabled", NULL, NULL, (BYTE*)&enc, &size);
            bool_settings["encryption_enabled"] = enc != 0;
            RegCloseKey(hKey);
        }
        bool_settings["first_run"] = true; // Default
#elif defined(__linux__)
        GKeyFile* keyfile = g_key_file_new();
        GError* error = NULL;
        if (g_key_file_load_from_file(keyfile, (g_get_user_config_dir() + std::string("/bluebeam/settings.ini")).c_str(), G_KEY_FILE_NONE, &error)) {
            gsize length;
            gchar* value = g_key_file_get_string(keyfile, "profile", "user_name", NULL);
            if (value) {
                string_settings["user_name"] = value;
                g_free(value);
            }
            // Similar for others
            bool_settings["encryption_enabled"] = g_key_file_get_boolean(keyfile, "security", "encryption_enabled", NULL);
            bool_settings["first_run"] = g_key_file_get_boolean(keyfile, "app", "first_run", NULL);
        } else {
            bool_settings["first_run"] = true; // Default
        }
        g_key_file_free(keyfile);
#endif
    }
};

Settings::Settings() : pimpl(std::make_unique<Impl>()) {}
Settings::~Settings() = default;

void Settings::set_user_name(const std::string& name) { pimpl->string_settings["user_name"] = name; }
std::string Settings::get_user_name() { return pimpl->string_settings["user_name"]; }

void Settings::set_theme(const std::string& theme) { pimpl->string_settings["theme"] = theme; }
std::string Settings::get_theme() { return pimpl->string_settings["theme"]; }

void Settings::set_download_path(const std::string& path) { pimpl->string_settings["download_path"] = path; }
std::string Settings::get_download_path() { return pimpl->string_settings["download_path"]; }

void Settings::set_encryption_enabled(bool enabled) { pimpl->bool_settings["encryption_enabled"] = enabled; }
bool Settings::get_encryption_enabled() { return pimpl->bool_settings["encryption_enabled"]; }

void Settings::add_trusted_device(const std::string& device_id) { pimpl->trusted_devices.push_back(device_id); }
void Settings::remove_trusted_device(const std::string& device_id) {
    pimpl->trusted_devices.erase(std::remove(pimpl->trusted_devices.begin(), pimpl->trusted_devices.end(), device_id), pimpl->trusted_devices.end());
}
std::vector<std::string> Settings::get_trusted_devices() { return pimpl->trusted_devices; }

void Settings::set_first_run(bool first) { pimpl->bool_settings["first_run"] = first; }
bool Settings::is_first_run() { return pimpl->bool_settings["first_run"]; }

void Settings::save() { pimpl->save(); }
void Settings::load() { pimpl->load(); }