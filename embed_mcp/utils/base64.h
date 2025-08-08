/*
 * Base64 encoding/decoding (RFC 4648)
 * Simple and reliable implementation
 * Based on public domain algorithms
 */

#ifndef BASE64_H
#define BASE64_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Base64 encode */
size_t base64_encode(const unsigned char *src, size_t len, char *out, size_t out_len);

/* Base64 decode */
size_t base64_decode(const char *src, size_t len, unsigned char *out, size_t out_len);

/* Calculate encoded size */
size_t base64_encoded_size(size_t len);

/* Calculate decoded size */
size_t base64_decoded_size(const char *src, size_t len);

#ifdef __cplusplus
}
#endif

#ifdef BASE64_IMPLEMENTATION

static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const int base64_decode_table[128] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-1,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};

size_t base64_encoded_size(size_t len) {
    return ((len + 2) / 3) * 4;
}

size_t base64_decoded_size(const char *src, size_t len) {
    if (len == 0) return 0;
    
    size_t padding = 0;
    if (src[len - 1] == '=') padding++;
    if (len > 1 && src[len - 2] == '=') padding++;
    
    return (len * 3) / 4 - padding;
}

size_t base64_encode(const unsigned char *src, size_t len, char *out, size_t out_len) {
    size_t encoded_len = base64_encoded_size(len);
    if (out_len < encoded_len + 1) return 0; // +1 for null terminator
    
    size_t i, j;
    for (i = 0, j = 0; i < len; i += 3, j += 4) {
        unsigned int a = src[i];
        unsigned int b = (i + 1 < len) ? src[i + 1] : 0;
        unsigned int c = (i + 2 < len) ? src[i + 2] : 0;
        
        unsigned int triple = (a << 16) + (b << 8) + c;
        
        out[j]     = base64_chars[(triple >> 18) & 63];
        out[j + 1] = base64_chars[(triple >> 12) & 63];
        out[j + 2] = (i + 1 < len) ? base64_chars[(triple >> 6) & 63] : '=';
        out[j + 3] = (i + 2 < len) ? base64_chars[triple & 63] : '=';
    }
    
    out[encoded_len] = '\0';
    return encoded_len;
}

size_t base64_decode(const char *src, size_t len, unsigned char *out, size_t out_len) {
    if (len % 4 != 0) return 0;
    
    size_t decoded_len = base64_decoded_size(src, len);
    if (out_len < decoded_len) return 0;
    
    size_t i, j;
    for (i = 0, j = 0; i < len; i += 4, j += 3) {
        int a = base64_decode_table[(int)src[i]];
        int b = base64_decode_table[(int)src[i + 1]];
        int c = (src[i + 2] == '=') ? 0 : base64_decode_table[(int)src[i + 2]];
        int d = (src[i + 3] == '=') ? 0 : base64_decode_table[(int)src[i + 3]];
        
        if (a == -1 || b == -1 || c == -1 || d == -1) return 0;
        
        unsigned int triple = (a << 18) + (b << 12) + (c << 6) + d;
        
        out[j] = (triple >> 16) & 255;
        if (src[i + 2] != '=') out[j + 1] = (triple >> 8) & 255;
        if (src[i + 3] != '=') out[j + 2] = triple & 255;
    }
    
    return decoded_len;
}

#endif /* BASE64_IMPLEMENTATION */

#endif /* BASE64_H */
