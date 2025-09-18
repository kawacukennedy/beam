#include "crypto_manager.h"
#include <stdio.h>
#include <sodium.h>
#include <string.h>

void crypto_manager_init(void) {
    if (sodium_init() == -1) {
        fprintf(stderr, "libsodium initialization failed\n");
    } else {
        printf("libsodium initialized successfully.\n");
    }
}

bool crypto_generate_session_key(unsigned char* key_buffer, size_t key_buffer_len) {
    if (key_buffer_len < crypto_secretbox_KEYBYTES) {
        fprintf(stderr, "Key buffer too small for session key. Required: %zu, Provided: %zu\n", crypto_secretbox_KEYBYTES, key_buffer_len);
        return false;
    }
    randombytes_buf(key_buffer, crypto_secretbox_KEYBYTES);
    printf("Session key generated.\n");
    return true;
}

bool crypto_encrypt_message(const unsigned char* message, size_t message_len,
                            const unsigned char* key,
                            unsigned char* ciphertext_buffer, size_t ciphertext_buffer_len,
                            size_t* actual_ciphertext_len) {
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));

    size_t required_len = crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + message_len;
    if (ciphertext_buffer_len < required_len) {
        fprintf(stderr, "Ciphertext buffer too small for encrypted message. Required: %zu, Provided: %zu\n", required_len, ciphertext_buffer_len);
        return false;
    }

    memcpy(ciphertext_buffer, nonce, sizeof(nonce));

    if (crypto_secretbox_easy(ciphertext_buffer + crypto_secretbox_NONCEBYTES,
                              message,
                              message_len,
                              nonce,
                              key) != 0) {
        fprintf(stderr, "Message encryption failed.\n");
        return false;
    }

    *actual_ciphertext_len = required_len;
    printf("Message encrypted. Ciphertext length: %zu\n", *actual_ciphertext_len);
    return true;
}

bool crypto_decrypt_message(const unsigned char* ciphertext, size_t ciphertext_len,
                            const unsigned char* key,
                            unsigned char* plaintext_buffer, size_t plaintext_buffer_len,
                            size_t* actual_plaintext_len) {
    if (ciphertext_len < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES) {
        fprintf(stderr, "Ciphertext too short for decryption.\n");
        return false;
    }

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    memcpy(nonce, ciphertext, sizeof(nonce));

    size_t encrypted_part_len = ciphertext_len - crypto_secretbox_NONCEBYTES;
    size_t max_plaintext_len = encrypted_part_len - crypto_secretbox_MACBYTES;

    if (plaintext_buffer_len < max_plaintext_len) {
        fprintf(stderr, "Plaintext buffer too small for decrypted message. Required: %zu, Provided: %zu\n", max_plaintext_len, plaintext_buffer_len);
        return false;
    }

    if (crypto_secretbox_open_easy(plaintext_buffer,
                                   ciphertext + crypto_secretbox_NONCEBYTES,
                                   encrypted_part_len,
                                   nonce,
                                   key) != 0) {
        fprintf(stderr, "Message decryption failed (bad key or corrupt data).\n");
        return false;
    }

    *actual_plaintext_len = max_plaintext_len;
    printf("Message decrypted. Plaintext length: %zu\n", *actual_plaintext_len);
    return true;
}

bool crypto_encrypt_file_chunk(const unsigned char* chunk, size_t chunk_len,
                               const unsigned char* key, unsigned long long chunk_index,
                               unsigned char* ciphertext_buffer, size_t ciphertext_buffer_len,
                               size_t* actual_ciphertext_len) {
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    memset(nonce, 0, sizeof(nonce));
    memcpy(nonce, &chunk_index, sizeof(chunk_index));
    randombytes_buf(nonce + sizeof(chunk_index), sizeof(nonce) - sizeof(chunk_index));

    size_t required_len = crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES + chunk_len;
    if (ciphertext_buffer_len < required_len) {
        fprintf(stderr, "Ciphertext buffer too small for encrypted file chunk. Required: %zu, Provided: %zu\n", required_len, ciphertext_buffer_len);
        return false;
    }

    memcpy(ciphertext_buffer, nonce, sizeof(nonce));

    if (crypto_secretbox_easy(ciphertext_buffer + crypto_secretbox_NONCEBYTES,
                              chunk,
                              chunk_len,
                              nonce,
                              key) != 0) {
        fprintf(stderr, "File chunk encryption failed.\n");
        return false;
    }

    *actual_ciphertext_len = required_len;
    printf("File chunk %llu encrypted. Ciphertext length: %zu\n", chunk_index, *actual_ciphertext_len);
    return true;
}

bool crypto_decrypt_file_chunk(const unsigned char* ciphertext, size_t ciphertext_len,
                               const unsigned char* key, unsigned long long chunk_index,
                               unsigned char* plaintext_buffer, size_t plaintext_buffer_len,
                               size_t* actual_plaintext_len) {
    if (ciphertext_len < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES) {
        fprintf(stderr, "Ciphertext too short for file chunk decryption.\n");
        return false;
    }

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    memcpy(nonce, ciphertext, sizeof(nonce));

    size_t encrypted_part_len = ciphertext_len - crypto_secretbox_NONCEBYTES;
    size_t max_plaintext_len = encrypted_part_len - crypto_secretbox_MACBYTES;

    if (plaintext_buffer_len < max_plaintext_len) {
        fprintf(stderr, "Plaintext buffer too small for decrypted file chunk. Required: %zu, Provided: %2zu\n", max_plaintext_len, plaintext_buffer_len);
        return false;
    }

    if (crypto_secretbox_open_easy(plaintext_buffer,
                                   ciphertext + crypto_secretbox_NONCEBYTES,
                                   encrypted_part_len,
                                   nonce,
                                   key) != 0) {
        fprintf(stderr, "File chunk decryption failed (bad key or corrupt data).\n");
        return false;
    }

    *actual_plaintext_len = max_plaintext_len;
    printf("File chunk %llu decrypted. Plaintext length: %zu\n", chunk_index, *actual_plaintext_len);
    return true;
}

bool crypto_generate_keypair(unsigned char* public_key, unsigned char* secret_key) {
    if (crypto_box_keypair(public_key, secret_key) != 0) {
        fprintf(stderr, "Failed to generate Curve25519 key pair.\n");
        return false;
    }
    printf("Curve25519 key pair generated.\n");
    return true;
}

bool crypto_perform_ecdh(unsigned char* shared_secret,
                         const unsigned char* public_key_remote,
                         const unsigned char* secret_key_local) {
    if (crypto_scalarmult(shared_secret, secret_key_local, public_key_remote) != 0) {
        fprintf(stderr, "Failed to perform Curve25519 ECDH.\n");
        return false;
    }
    printf("Curve25519 ECDH shared secret derived.\n");
    return true;
}