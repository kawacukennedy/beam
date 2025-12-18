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

#include <filesystem>
namespace fs = std::filesystem;

extern "C" int crypto_encrypt(const uint8_t* data, size_t len, uint8_t* out);
extern "C" int crypto_decrypt(const uint8_t* data, size_t len, uint8_t* out);

class Settings::Impl {
public:
    std::map<std::string, std::string> string_settings;
    std::map<std::string, bool> bool_settings;
    std::map<std::string, int> int_settings;
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
        CFPreferencesSetAppValue(CFSTR("auto_lock_timeout"), CFNumberCreate(NULL, kCFNumberIntType, &int_settings["auto_lock_timeout"]), appID);
        CFPreferencesSetAppValue(CFSTR("biometric_auth_enabled"), bool_settings["biometric_auth_enabled"] ? kCFBooleanTrue : kCFBooleanFalse, appID);
        CFPreferencesSetAppValue(CFSTR("two_factor_enabled"), bool_settings["two_factor_enabled"] ? kCFBooleanTrue : kCFBooleanFalse, appID);
        CFPreferencesSetAppValue(CFSTR("language"), CFStringCreateWithCString(NULL, string_settings["language"].c_str(), kCFStringEncodingUTF8), appID);
        CFPreferencesSetAppValue(CFSTR("notifications_enabled"), bool_settings["notifications_enabled"] ? kCFBooleanTrue : kCFBooleanFalse, appID);
        CFPreferencesSetAppValue(CFSTR("auto_update_enabled"), bool_settings["auto_update_enabled"] ? kCFBooleanTrue : kCFBooleanFalse, appID);
        CFPreferencesSetAppValue(CFSTR("profile_picture_path"), CFStringCreateWithCString(NULL, string_settings["profile_picture_path"].c_str(), kCFStringEncodingUTF8), appID);
        CFPreferencesSetAppValue(CFSTR("email"), CFStringCreateWithCString(NULL, string_settings["email"].c_str(), kCFStringEncodingUTF8), appID);
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
        RegSetValueEx(hKey, "auto_lock_timeout", 0, REG_DWORD, (BYTE*)&int_settings["auto_lock_timeout"], sizeof(DWORD));
        DWORD bio = bool_settings["biometric_auth_enabled"] ? 1 : 0;
        RegSetValueEx(hKey, "biometric_auth_enabled", 0, REG_DWORD, (BYTE*)&bio, sizeof(DWORD));
        DWORD two = bool_settings["two_factor_enabled"] ? 1 : 0;
        RegSetValueEx(hKey, "two_factor_enabled", 0, REG_DWORD, (BYTE*)&two, sizeof(DWORD));
        RegSetValueEx(hKey, "language", 0, REG_SZ, (BYTE*)string_settings["language"].c_str(), string_settings["language"].size() + 1);
        DWORD notif = bool_settings["notifications_enabled"] ? 1 : 0;
        RegSetValueEx(hKey, "notifications_enabled", 0, REG_DWORD, (BYTE*)&notif, sizeof(DWORD));
        DWORD auto_up = bool_settings["auto_update_enabled"] ? 1 : 0;
        RegSetValueEx(hKey, "auto_update_enabled", 0, REG_DWORD, (BYTE*)&auto_up, sizeof(DWORD));
        RegSetValueEx(hKey, "profile_picture_path", 0, REG_SZ, (BYTE*)string_settings["profile_picture_path"].c_str(), string_settings["profile_picture_path"].size() + 1);
        RegSetValueEx(hKey, "email", 0, REG_SZ, (BYTE*)string_settings["email"].c_str(), string_settings["email"].size() + 1);
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
        // Serialize to JSON-like string
        std::string data = "{";
        data += "\"user_name\":\"" + string_settings["user_name"] + "\",";
        data += "\"theme\":\"" + string_settings["theme"] + "\",";
        data += "\"download_path\":\"" + string_settings["download_path"] + "\",";
        data += "\"encryption_enabled\":" + (bool_settings["encryption_enabled"] ? "true" : "false") + ",";
        data += "\"auto_lock_timeout\":" + std::to_string(int_settings["auto_lock_timeout"]) + ",";
        data += "\"biometric_auth_enabled\":" + (bool_settings["biometric_auth_enabled"] ? "true" : "false") + ",";
        data += "\"two_factor_enabled\":" + (bool_settings["two_factor_enabled"] ? "true" : "false") + ",";
        data += "\"language\":\"" + string_settings["language"] + "\",";
        data += "\"notifications_enabled\":" + (bool_settings["notifications_enabled"] ? "true" : "false") + ",";
        data += "\"auto_update_enabled\":" + (bool_settings["auto_update_enabled"] ? "true" : "false") + ",";
        data += "\"profile_picture_path\":\"" + string_settings["profile_picture_path"] + "\",";
        data += "\"email\":\"" + string_settings["email"] + "\",";
        data += "\"first_run\":" + (bool_settings["first_run"] ? "true" : "false") + ",";
        data += "\"trusted_devices\":[";
        for (size_t i = 0; i < trusted_devices.size(); ++i) {
            data += "\"" + trusted_devices[i] + "\"";
            if (i < trusted_devices.size() - 1) data += ",";
        }
        data += "]}";

        // Encrypt data
        std::vector<uint8_t> encrypted(data.size() + 16); // Extra for AES
        int enc_len = crypto_encrypt((uint8_t*)data.data(), data.size(), encrypted.data());
        if (enc_len > 0) {
            std::ofstream file(g_get_user_config_dir() + std::string("/bluebeam/settings.enc"), std::ios::binary);
            file.write((char*)encrypted.data(), enc_len);
            file.close();
        }
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
        // Load other settings similarly
        CFNumberRef num;
        num = (CFNumberRef)CFPreferencesCopyAppValue(CFSTR("auto_lock_timeout"), appID);
        if (num) {
            int val;
            CFNumberGetValue(num, kCFNumberIntType, &val);
            int_settings["auto_lock_timeout"] = val;
            CFRelease(num);
        } else {
            int_settings["auto_lock_timeout"] = 5; // Default
        }
        CFBooleanRef bool_val;
        bool_val = (CFBooleanRef)CFPreferencesCopyAppValue(CFSTR("biometric_auth_enabled"), appID);
        bool_settings["biometric_auth_enabled"] = bool_val && CFBooleanGetValue(bool_val);
        if (bool_val) CFRelease(bool_val);
        bool_val = (CFBooleanRef)CFPreferencesCopyAppValue(CFSTR("two_factor_enabled"), appID);
        bool_settings["two_factor_enabled"] = bool_val && CFBooleanGetValue(bool_val);
        if (bool_val) CFRelease(bool_val);
        value = (CFStringRef)CFPreferencesCopyAppValue(CFSTR("language"), appID);
        if (value) {
            char buffer[256];
            CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8);
            string_settings["language"] = buffer;
            CFRelease(value);
        } else {
            string_settings["language"] = "en"; // Default
        }
        bool_val = (CFBooleanRef)CFPreferencesCopyAppValue(CFSTR("notifications_enabled"), appID);
        bool_settings["notifications_enabled"] = bool_val && CFBooleanGetValue(bool_val);
        if (bool_val) CFRelease(bool_val);
        bool_val = (CFBooleanRef)CFPreferencesCopyAppValue(CFSTR("auto_update_enabled"), appID);
        bool_settings["auto_update_enabled"] = bool_val && CFBooleanGetValue(bool_val);
        if (bool_val) CFRelease(bool_val);
        value = (CFStringRef)CFPreferencesCopyAppValue(CFSTR("profile_picture_path"), appID);
        if (value) {
            char buffer[256];
            CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8);
            string_settings["profile_picture_path"] = buffer;
            CFRelease(value);
        }
        value = (CFStringRef)CFPreferencesCopyAppValue(CFSTR("email"), appID);
        if (value) {
            char buffer[256];
            CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8);
            string_settings["email"] = buffer;
            CFRelease(value);
        }
        bool_val = (CFBooleanRef)CFPreferencesCopyAppValue(CFSTR("first_run"), appID);
        bool_settings["first_run"] = bool_val ? CFBooleanGetValue(bool_val) : true; // Default
        if (bool_val) CFRelease(bool_val);
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
            DWORD timeout;
            RegQueryValueEx(hKey, "auto_lock_timeout", NULL, NULL, (BYTE*)&timeout, &size);
            int_settings["auto_lock_timeout"] = timeout;
            DWORD bio;
            RegQueryValueEx(hKey, "biometric_auth_enabled", NULL, NULL, (BYTE*)&bio, &size);
            bool_settings["biometric_auth_enabled"] = bio != 0;
            DWORD two;
            RegQueryValueEx(hKey, "two_factor_enabled", NULL, NULL, (BYTE*)&two, &size);
            bool_settings["two_factor_enabled"] = two != 0;
            char lang[256];
            size = sizeof(lang);
            RegQueryValueEx(hKey, "language", NULL, NULL, (BYTE*)lang, &size);
            string_settings["language"] = lang;
            DWORD notif;
            RegQueryValueEx(hKey, "notifications_enabled", NULL, NULL, (BYTE*)&notif, &size);
            bool_settings["notifications_enabled"] = notif != 0;
            DWORD auto_up;
            RegQueryValueEx(hKey, "auto_update_enabled", NULL, NULL, (BYTE*)&auto_up, &size);
            bool_settings["auto_update_enabled"] = auto_up != 0;
            char pic_path[256];
            size = sizeof(pic_path);
            RegQueryValueEx(hKey, "profile_picture_path", NULL, NULL, (BYTE*)pic_path, &size);
            string_settings["profile_picture_path"] = pic_path;
            char email[256];
            size = sizeof(email);
            RegQueryValueEx(hKey, "email", NULL, NULL, (BYTE*)email, &size);
            string_settings["email"] = email;
            DWORD first;
            RegQueryValueEx(hKey, "first_run", NULL, NULL, (BYTE*)&first, &size);
            bool_settings["first_run"] = first != 0;
            RegCloseKey(hKey);
        } else {
            // Defaults
            int_settings["auto_lock_timeout"] = 5;
            bool_settings["biometric_auth_enabled"] = false;
            bool_settings["two_factor_enabled"] = false;
            string_settings["language"] = "en";
            bool_settings["notifications_enabled"] = true;
            bool_settings["auto_update_enabled"] = true;
            bool_settings["first_run"] = true;
        }
#elif defined(__linux__)
        std::string config_dir = g_get_user_config_dir();
        std::string file_path = config_dir + "/bluebeam/settings.enc";
        std::ifstream file(file_path, std::ios::binary);
        if (file) {
            std::vector<uint8_t> encrypted((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            std::vector<uint8_t> decrypted(encrypted.size());
            int dec_len = crypto_decrypt(encrypted.data(), encrypted.size(), decrypted.data());
            if (dec_len > 0) {
                std::string data((char*)decrypted.data(), dec_len);
                // Parse JSON-like string (simple parsing)
                // For simplicity, assume it's JSON and use a library, but since no JSON lib, skip parsing for now
                // In real app, use nlohmann/json or similar
                // For now, set defaults
            }
        }
        // Set defaults if not loaded
        if (string_settings.find("user_name") == string_settings.end()) {
            int_settings["auto_lock_timeout"] = 5;
            bool_settings["biometric_auth_enabled"] = false;
            bool_settings["two_factor_enabled"] = false;
            string_settings["language"] = "en";
            bool_settings["notifications_enabled"] = true;
            bool_settings["auto_update_enabled"] = true;
            bool_settings["first_run"] = true;
        }
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

void Settings::set_language(const std::string& lang) { pimpl->string_settings["language"] = lang; }
std::string Settings::get_language() { return pimpl->string_settings["language"]; }

void Settings::set_notifications_enabled(bool enabled) { pimpl->bool_settings["notifications_enabled"] = enabled; }
bool Settings::get_notifications_enabled() { return pimpl->bool_settings["notifications_enabled"]; }

void Settings::set_auto_update_enabled(bool enabled) { pimpl->bool_settings["auto_update_enabled"] = enabled; }
bool Settings::get_auto_update_enabled() { return pimpl->bool_settings["auto_update_enabled"]; }

void Settings::set_profile_picture_path(const std::string& path) { pimpl->string_settings["profile_picture_path"] = path; }
std::string Settings::get_profile_picture_path() { return pimpl->string_settings["profile_picture_path"]; }

void Settings::set_email(const std::string& email) { pimpl->string_settings["email"] = email; }
std::string Settings::get_email() { return pimpl->string_settings["email"]; }

void Settings::set_encryption_enabled(bool enabled) { pimpl->bool_settings["encryption_enabled"] = enabled; }
bool Settings::get_encryption_enabled() { return pimpl->bool_settings["encryption_enabled"]; }

void Settings::set_auto_lock_timeout(int minutes) { pimpl->int_settings["auto_lock_timeout"] = minutes; }
int Settings::get_auto_lock_timeout() { return pimpl->int_settings["auto_lock_timeout"]; }

void Settings::set_biometric_auth_enabled(bool enabled) { pimpl->bool_settings["biometric_auth_enabled"] = enabled; }
bool Settings::get_biometric_auth_enabled() { return pimpl->bool_settings["biometric_auth_enabled"]; }

void Settings::set_two_factor_enabled(bool enabled) { pimpl->bool_settings["two_factor_enabled"] = enabled; }
bool Settings::get_two_factor_enabled() { return pimpl->bool_settings["two_factor_enabled"]; }

void Settings::add_trusted_device(const std::string& device_id) { pimpl->trusted_devices.push_back(device_id); }
void Settings::remove_trusted_device(const std::string& device_id) {
    pimpl->trusted_devices.erase(std::remove(pimpl->trusted_devices.begin(), pimpl->trusted_devices.end(), device_id), pimpl->trusted_devices.end());
}
std::vector<std::string> Settings::get_trusted_devices() { return pimpl->trusted_devices; }

void Settings::set_first_run(bool first) { pimpl->bool_settings["first_run"] = first; }
bool Settings::is_first_run() { return pimpl->bool_settings["first_run"]; }

void Settings::save() { pimpl->save(); }
void Settings::load() { pimpl->load(); }

std::string Settings::get_app_data_path() {
#if defined(__APPLE__)
    // Use ~/Library/Application Support/BlueBeam
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

std::string Settings::get_temp_path() {
#if defined(__APPLE__) || defined(__linux__)
    return "/tmp";
#elif defined(_WIN32)
    char path[MAX_PATH];
    GetTempPathA(MAX_PATH, path);
    return std::string(path);
#endif
    return "";
}

std::string Settings::get_documents_path() {
#if defined(__APPLE__)
    return std::string(getenv("HOME")) + "/Documents";
#elif defined(_WIN32)
    char path[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, path) == S_OK) {
        return std::string(path);
    }
    return "";
#elif defined(__linux__)
    const char* docs_dir = getenv("XDG_DOCUMENTS_DIR");
    if (docs_dir) {
        return std::string(docs_dir);
    } else {
        return std::string(getenv("HOME")) + "/Documents";
    }
#endif
    return "";
}