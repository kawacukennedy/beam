#include "crypto.h"
#include <memory>
#include <cstring>
#include <map>

extern "C" {
    void* crypto_new();
    void crypto_free(void* ptr);
    void crypto_get_ecdh_public_key(void* ptr, uint8_t* out);
    char* crypto_get_rsa_public_key_pem(void* ptr);
    void crypto_derive_shared_secret(void* ptr, const uint8_t* peer_public, uint8_t* out);
    void crypto_set_session_key(void* ptr, const char* session_id, const uint8_t* key);
    uint8_t* crypto_encrypt_message(void* ptr, const char* session_id, const uint8_t* data, size_t len, size_t* out_len);
    uint8_t* crypto_decrypt_message(void* ptr, const char* session_id, const uint8_t* data, size_t len, size_t* out_len);
    char* crypto_calculate_checksum(void* ptr, const uint8_t* data, size_t len);
}

class Crypto::Impl {
public:
    void* crypto_mgr;
    std::map<std::string, std::array<uint8_t, 32>> session_keys;

    Impl() : crypto_mgr(crypto_new()) {}

    ~Impl() {
        crypto_free(crypto_mgr);
    }
};

Crypto::Crypto() : pimpl(std::make_unique<Impl>()) {}
Crypto::~Crypto() = default;

std::array<uint8_t, 32> Crypto::get_ecdh_public_key() {
    std::array<uint8_t, 32> key;
    crypto_get_ecdh_public_key(pimpl->crypto_mgr, key.data());
    return key;
}

std::string Crypto::get_rsa_public_key_pem() {
    char* pem = crypto_get_rsa_public_key_pem(pimpl->crypto_mgr);
    std::string result(pem);
    // Free the C string if needed, but Rust manages it
    return result;
}

std::array<uint8_t, 32> Crypto::derive_shared_secret(const std::array<uint8_t, 32>& peer_public) {
    std::array<uint8_t, 32> secret;
    crypto_derive_shared_secret(pimpl->crypto_mgr, peer_public.data(), secret.data());
    return secret;
}

void Crypto::set_session_key(const std::string& session_id, const std::array<uint8_t, 32>& key) {
    crypto_set_session_key(pimpl->crypto_mgr, session_id.c_str(), key.data());
}

std::vector<uint8_t> Crypto::encrypt_message(const std::string& session_id, const std::vector<uint8_t>& data) {
    size_t out_len;
    uint8_t* encrypted = crypto_encrypt_message(pimpl->crypto_mgr, session_id.c_str(), data.data(), data.size(), &out_len);
    if (encrypted) {
        std::vector<uint8_t> result(encrypted, encrypted + out_len);
        // Free if needed
        return result;
    }
    return {};
}

std::vector<uint8_t> Crypto::decrypt_message(const std::string& session_id, const std::vector<uint8_t>& data) {
    size_t out_len;
    uint8_t* decrypted = crypto_decrypt_message(pimpl->crypto_mgr, session_id.c_str(), data.data(), data.size(), &out_len);
    if (decrypted) {
        std::vector<uint8_t> result(decrypted, decrypted + out_len);
        // Free if needed
        return result;
    }
    return {};
}

std::string Crypto::calculate_checksum(const std::vector<uint8_t>& data) {
    char* checksum = crypto_calculate_checksum(pimpl->crypto_mgr, data.data(), data.size());
    std::string result(checksum);
    // Free if needed
    return result;
}