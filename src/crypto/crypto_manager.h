#ifndef CRYPTO_MANAGER_H
#define CRYPTO_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Function to initialize the crypto manager
void crypto_manager_init(void);

// Function to generate a new session key
bool crypto_generate_session_key(unsigned char* key_buffer, size_t key_buffer_len);

// Function to encrypt a message
bool crypto_encrypt_message(const unsigned char* message, size_t message_len,
                            const unsigned char* key,
                            unsigned char* ciphertext_buffer, size_t ciphertext_buffer_len,
                            size_t* actual_ciphertext_len);

// Function to decrypt a message
bool crypto_decrypt_message(const unsigned char* ciphertext, size_t ciphertext_len,
                            const unsigned char* key,
                            unsigned char* plaintext_buffer, size_t plaintext_buffer_len,
                            size_t* actual_plaintext_len);

// Function to encrypt a file chunk
bool crypto_encrypt_file_chunk(const unsigned char* chunk, size_t chunk_len,
                               const unsigned char* key, unsigned long long chunk_index,
                               unsigned char* ciphertext_buffer, size_t ciphertext_buffer_len,
                               size_t* actual_ciphertext_len);

// Function to decrypt a file chunk
bool crypto_decrypt_file_chunk(const unsigned char* ciphertext, size_t ciphertext_len,
                               const unsigned char* key, unsigned long long chunk_index,
                               unsigned char* plaintext_buffer, size_t plaintext_buffer_len,
                               size_t* actual_plaintext_len);

// Function to generate a Curve25519 key pair
bool crypto_generate_keypair(unsigned char* public_key, unsigned char* secret_key);

// Function to perform Curve25519 ECDH to derive a shared secret
bool crypto_perform_ecdh(unsigned char* shared_secret,
                         const unsigned char* public_key_remote,
                         const unsigned char* secret_key_local);

#ifdef __cplusplus
}
#endif

#endif // CRYPTO_MANAGER_H