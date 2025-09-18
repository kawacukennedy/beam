#ifndef BASE64_H
#define BASE64_H

#include <stddef.h>

// Encodes a byte array into a base64 string.
// Returns a newly allocated string that must be freed by the caller.
char* base64_encode(const unsigned char* data, size_t input_length);

// Decodes a base64 string into a byte array.
// Returns a newly allocated byte array that must be freed by the caller.
unsigned char* base64_decode(const char* data, size_t input_length, size_t* output_length);

#endif // BASE64_H
