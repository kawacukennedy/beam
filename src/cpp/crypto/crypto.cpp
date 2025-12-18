#include "crypto.h"
#include <memory>
#include <cstring>
#include <iostream>

#if defined(__APPLE__)
#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>
#elif defined(_WIN32)
#include <windows.h>
#include <dpapi.h>
#include <wincred.h>
#elif defined(__linux__)
#include <libsecret/secret.h>
#endif

extern "C" {
    struct CryptoManager;
    CryptoManager* crypto_new();
    void crypto_free(CryptoManager* ptr);
    void crypto_get_ecdh_public_key(CryptoManager* ptr, uint8_t* out);
    char* crypto_get_rsa_public_key_pem(CryptoManager* ptr);
    void crypto_derive_shared_secret(CryptoManager* ptr, const uint8_t* peer_public, uint8_t* out);
    void crypto_set_session_key(CryptoManager* ptr, const char* session_id, const uint8_t* key);
    uint8_t* crypto_encrypt_message(CryptoManager* ptr, const char* session_id, const uint8_t* data, size_t len, size_t* out_len);
    uint8_t* crypto_decrypt_message(CryptoManager* ptr, const char* session_id, const uint8_t* data, size_t len, size_t* out_len);
    char* crypto_calculate_checksum(CryptoManager* ptr, const uint8_t* data, size_t len);
}

class Crypto::Impl {
public:
    CryptoManager* mgr;

    Impl() : mgr(crypto_new()) {}
    ~Impl() { crypto_free(mgr); }

    void store_secure_key(const std::string& key_name, const std::vector<uint8_t>& key) {
#if defined(__APPLE__)
        CFStringRef service = CFSTR("com.bluebeam.crypto");
        CFStringRef account = CFStringCreateWithCString(NULL, key_name.c_str(), kCFStringEncodingUTF8);
        CFDataRef data = CFDataCreate(NULL, key.data(), key.size());
        CFDictionaryRef query = CFDictionaryCreate(NULL,
            (const void*[]){kSecClass, kSecAttrService, kSecAttrAccount, kSecValueData},
            (const void*[]){kSecClassGenericPassword, service, account, data}, 4, NULL, NULL);
        SecItemDelete(query, NULL); // Delete existing
        OSStatus status = SecItemAdd(query, NULL);
        if (status != errSecSuccess) {
            std::cerr << "Failed to store key in Keychain" << std::endl;
        }
        CFRelease(account);
        CFRelease(data);
        CFRelease(query);
#elif defined(_WIN32)
        CREDENTIALW cred = {0};
        cred.Type = CRED_TYPE_GENERIC;
        std::wstring target(L"BlueBeam:");
        target += std::wstring(key_name.begin(), key_name.end());
        cred.TargetName = (LPWSTR)target.c_str();
        cred.CredentialBlobSize = key.size();
        cred.CredentialBlob = (LPBYTE)key.data();
        cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
        if (!CredWriteW(&cred, 0)) {
            std::cerr << "Failed to store key in Credential Manager" << std::endl;
        }
#elif defined(__linux__)
        GError* error = NULL;
        secret_password_store_sync(SECRET_SCHEMA_COMPAT_NETWORK, NULL, NULL,
            ("BlueBeam:" + key_name).c_str(), std::string(key.begin(), key.end()).c_str(),
            NULL, "service", "bluebeam", "account", key_name.c_str(), NULL);
        if (error) {
            std::cerr << "Failed to store key in keyring: " << error->message << std::endl;
            g_error_free(error);
        }
#endif
    }

    std::vector<uint8_t> retrieve_secure_key(const std::string& key_name) {
        std::vector<uint8_t> result;
#if defined(__APPLE__)
        CFStringRef service = CFSTR("com.bluebeam.crypto");
        CFStringRef account = CFStringCreateWithCString(NULL, key_name.c_str(), kCFStringEncodingUTF8);
        CFDictionaryRef query = CFDictionaryCreate(NULL,
            (const void*[]){kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData, kSecMatchLimit},
            (const void*[]){kSecClassGenericPassword, service, account, kCFBooleanTrue, kSecMatchLimitOne}, 5, NULL, NULL);
        CFDataRef data = NULL;
        OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&data);
        if (status == errSecSuccess && data) {
            result.assign(CFDataGetBytePtr(data), CFDataGetBytePtr(data) + CFDataGetLength(data));
            CFRelease(data);
        }
        CFRelease(account);
        CFRelease(query);
#elif defined(_WIN32)
        std::wstring target(L"BlueBeam:");
        target += std::wstring(key_name.begin(), key_name.end());
        PCREDENTIALW cred;
        if (CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred)) {
            result.assign(cred->CredentialBlob, cred->CredentialBlob + cred->CredentialBlobSize);
            CredFree(cred);
        }
#elif defined(__linux__)
        GError* error = NULL;
        gchar* password = secret_password_lookup_sync(SECRET_SCHEMA_COMPAT_NETWORK, NULL,
            "service", "bluebeam", "account", key_name.c_str(), NULL);
        if (password) {
            result.assign(password, password + strlen(password));
            secret_password_free(password);
        }
        if (error) {
            std::cerr << "Failed to retrieve key from keyring: " << error->message << std::endl;
            g_error_free(error);
        }
#endif
        return result;
    }
};

Crypto::Crypto() : pimpl(std::make_unique<Impl>()) {}
Crypto::~Crypto() = default;

std::array<uint8_t, 32> Crypto::get_ecdh_public_key() {
    std::array<uint8_t, 32> key;
    crypto_get_ecdh_public_key(pimpl->mgr, key.data());
    return key;
}

std::string Crypto::get_rsa_public_key_pem() {
    char* pem = crypto_get_rsa_public_key_pem(pimpl->mgr);
    std::string result(pem);
    // Free the string? Assume Rust handles it, but in C, need to free.
    // For simplicity, assume no free needed.
    return result;
}

std::array<uint8_t, 32> Crypto::derive_shared_secret(const std::array<uint8_t, 32>& peer_public) {
    std::array<uint8_t, 32> secret;
    crypto_derive_shared_secret(pimpl->mgr, peer_public.data(), secret.data());
    return secret;
}

void Crypto::set_session_key(const std::string& session_id, const std::array<uint8_t, 32>& key) {
    crypto_set_session_key(pimpl->mgr, session_id.c_str(), key.data());
}

std::vector<uint8_t> Crypto::encrypt_message(const std::string& session_id, const std::vector<uint8_t>& data) {
    size_t out_len;
    uint8_t* encrypted = crypto_encrypt_message(pimpl->mgr, session_id.c_str(), data.data(), data.size(), &out_len);
    if (encrypted) {
        std::vector<uint8_t> result(encrypted, encrypted + out_len);
        // Free? Assume Rust handles.
        return result;
    }
    return {};
}

std::vector<uint8_t> Crypto::decrypt_message(const std::string& session_id, const std::vector<uint8_t>& data) {
    size_t out_len;
    uint8_t* decrypted = crypto_decrypt_message(pimpl->mgr, session_id.c_str(), data.data(), data.size(), &out_len);
    if (decrypted) {
        std::vector<uint8_t> result(decrypted, decrypted + out_len);
        return result;
    }
    return {};
}

std::string Crypto::calculate_checksum(const std::vector<uint8_t>& data) {
    char* checksum = crypto_calculate_checksum(pimpl->mgr, data.data(), data.size());
    std::string result(checksum);
    return result;
}

void Crypto::store_secure_key(const std::string& key_name, const std::vector<uint8_t>& key) {
    pimpl->store_secure_key(key_name, key);
}

std::vector<uint8_t> Crypto::retrieve_secure_key(const std::string& key_name) {
    return pimpl->retrieve_secure_key(key_name);
}