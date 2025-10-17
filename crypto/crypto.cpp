#include "crypto.h"
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <memory>
#include <cstring>
#include <map>

class Crypto::Impl {
public:
    EVP_PKEY *ecdh_key;
    RSA *rsa_key;
    std::map<std::string, std::array<uint8_t, 32>> session_keys;

    Impl() : ecdh_key(nullptr), rsa_key(nullptr) {
        // Generate ECDH key
        EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr);
        EVP_PKEY_keygen_init(pctx);
        EVP_PKEY_keygen(pctx, &ecdh_key);
        EVP_PKEY_CTX_free(pctx);

        // Generate RSA key
        rsa_key = RSA_new();
        BIGNUM *e = BN_new();
        BN_set_word(e, RSA_F4);
        RSA_generate_key_ex(rsa_key, 4096, e, nullptr);
        BN_free(e);
    }

    ~Impl() {
        if (ecdh_key) EVP_PKEY_free(ecdh_key);
        if (rsa_key) RSA_free(rsa_key);
    }
};

Crypto::Crypto() : pimpl(std::make_unique<Impl>()) {}
Crypto::~Crypto() = default;

std::array<uint8_t, 32> Crypto::get_ecdh_public_key() {
    std::array<uint8_t, 32> key;
    size_t len = 32;
    EVP_PKEY_get_raw_public_key(pimpl->ecdh_key, key.data(), &len);
    return key;
}

std::string Crypto::get_rsa_public_key_pem() {
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_RSA_PUBKEY(bio, pimpl->rsa_key);
    char *pem_data;
    long len = BIO_get_mem_data(bio, &pem_data);
    std::string result(pem_data, len);
    BIO_free(bio);
    return result;
}

std::array<uint8_t, 32> Crypto::derive_shared_secret(const std::array<uint8_t, 32>& peer_public) {
    std::array<uint8_t, 32> secret;
    EVP_PKEY *peer_key = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, peer_public.data(), 32);
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pimpl->ecdh_key, nullptr);
    EVP_PKEY_derive_init(ctx);
    EVP_PKEY_derive_set_peer(ctx, peer_key);
    size_t len = 32;
    EVP_PKEY_derive(ctx, secret.data(), &len);
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(peer_key);
    return secret;
}

void Crypto::set_session_key(const std::string& session_id, const std::array<uint8_t, 32>& key) {
    pimpl->session_keys[session_id] = key;
}

std::vector<uint8_t> Crypto::encrypt_message(const std::string& session_id, const std::vector<uint8_t>& data) {
    auto it = pimpl->session_keys.find(session_id);
    if (it == pimpl->session_keys.end()) return {};

    std::vector<uint8_t> ciphertext;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    uint8_t iv[12];
    RAND_bytes(iv, 12);
    EVP_EncryptInit_ex(ctx, nullptr, nullptr, it->second.data(), iv);
    ciphertext.insert(ciphertext.end(), iv, iv + 12);

    int len;
    uint8_t out[1024];
    EVP_EncryptUpdate(ctx, out, &len, data.data(), data.size());
    ciphertext.insert(ciphertext.end(), out, out + len);

    EVP_EncryptFinal_ex(ctx, out, &len);
    ciphertext.insert(ciphertext.end(), out, out + len);

    uint8_t tag[16];
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);
    ciphertext.insert(ciphertext.end(), tag, tag + 16);

    EVP_CIPHER_CTX_free(ctx);
    return ciphertext;
}

std::vector<uint8_t> Crypto::decrypt_message(const std::string& session_id, const std::vector<uint8_t>& data) {
    auto it = pimpl->session_keys.find(session_id);
    if (it == pimpl->session_keys.end() || data.size() < 28) return {};

    std::vector<uint8_t> plaintext;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_DecryptInit_ex(ctx, nullptr, nullptr, it->second.data(), data.data());
    int len;
    uint8_t out[1024];
    EVP_DecryptUpdate(ctx, out, &len, data.data() + 12, data.size() - 28);
    plaintext.insert(plaintext.end(), out, out + len);

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void*)(data.data() + data.size() - 16));
    EVP_DecryptFinal_ex(ctx, out, &len);
    plaintext.insert(plaintext.end(), out, out + len);

    EVP_CIPHER_CTX_free(ctx);
    return plaintext;
}

std::string Crypto::calculate_checksum(const std::vector<uint8_t>& data) {
    uint8_t hash[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hash);
    char hex[65];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(hex + 2 * i, "%02x", hash[i]);
    }
    return std::string(hex);
}