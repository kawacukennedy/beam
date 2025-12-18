#pragma once
#include <vector>
#include <memory>
#include <string>
#include <array>

class Crypto {
public:
    Crypto();
    ~Crypto();

    std::array<uint8_t, 32> get_ecdh_public_key();
    std::string get_rsa_public_key_pem();
    std::array<uint8_t, 32> derive_shared_secret(const std::array<uint8_t, 32>& peer_public);
    void set_session_key(const std::string& session_id, const std::array<uint8_t, 32>& key);
    std::vector<uint8_t> encrypt_message(const std::string& session_id, const std::vector<uint8_t>& data);
    std::vector<uint8_t> decrypt_message(const std::string& session_id, const std::vector<uint8_t>& data);
    std::string calculate_checksum(const std::vector<uint8_t>& data);

    // Secure key storage
    void store_secure_key(const std::string& key_name, const std::vector<uint8_t>& key);
    std::vector<uint8_t> retrieve_secure_key(const std::string& key_name);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};