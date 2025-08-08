/*
 * UUID4 (Random UUID) Generator
 * Single header library
 */

#ifndef UUID4_H
#define UUID4_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t bytes[16];
} uuid4_t;

/* Generate a random UUID4 */
void uuid4_generate(uuid4_t *uuid);

/* Convert UUID to string (36 chars + null terminator) */
void uuid4_to_string(const uuid4_t *uuid, char *str);

/* Generate UUID4 string directly */
void uuid4_generate_string(char *str);

#ifdef __cplusplus
}
#endif

#ifdef UUID4_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static int uuid4_initialized = 0;

static void uuid4_init(void) {
    if (!uuid4_initialized) {
        srand((unsigned int)(time(NULL) ^ getpid()));
        uuid4_initialized = 1;
    }
}

static uint32_t uuid4_random(void) {
    return ((uint32_t)rand() << 16) | (uint32_t)rand();
}

void uuid4_generate(uuid4_t *uuid) {
    uuid4_init();
    
    /* Generate 16 random bytes */
    for (int i = 0; i < 16; i += 4) {
        uint32_t r = uuid4_random();
        uuid->bytes[i]     = (r >> 24) & 0xFF;
        uuid->bytes[i + 1] = (r >> 16) & 0xFF;
        uuid->bytes[i + 2] = (r >> 8) & 0xFF;
        uuid->bytes[i + 3] = r & 0xFF;
    }
    
    /* Set version (4) and variant bits */
    uuid->bytes[6] = (uuid->bytes[6] & 0x0F) | 0x40; /* Version 4 */
    uuid->bytes[8] = (uuid->bytes[8] & 0x3F) | 0x80; /* Variant 10 */
}

void uuid4_to_string(const uuid4_t *uuid, char *str) {
    sprintf(str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid->bytes[0], uuid->bytes[1], uuid->bytes[2], uuid->bytes[3],
        uuid->bytes[4], uuid->bytes[5],
        uuid->bytes[6], uuid->bytes[7],
        uuid->bytes[8], uuid->bytes[9],
        uuid->bytes[10], uuid->bytes[11], uuid->bytes[12], uuid->bytes[13], uuid->bytes[14], uuid->bytes[15]);
}

void uuid4_generate_string(char *str) {
    uuid4_t uuid;
    uuid4_generate(&uuid);
    uuid4_to_string(&uuid, str);
}

#endif /* UUID4_IMPLEMENTATION */

#endif /* UUID4_H */
