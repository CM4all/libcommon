/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef HEX_FORMAT_H
#define HEX_FORMAT_H

#include "util/Compiler.h"

#include <stdint.h>
#include <string.h>

extern const char hex_digits[0x10];

static gcc_always_inline void
format_uint8_hex_fixed(char dest[2], uint8_t number) {
    dest[0] = hex_digits[(number >> 4) & 0xf];
    dest[1] = hex_digits[number & 0xf];
}

static gcc_always_inline void
format_uint16_hex_fixed(char dest[4], uint16_t number) {
    dest[0] = hex_digits[(number >> 12) & 0xf];
    dest[1] = hex_digits[(number >> 8) & 0xf];
    dest[2] = hex_digits[(number >> 4) & 0xf];
    dest[3] = hex_digits[number & 0xf];
}

static gcc_always_inline void
format_uint32_hex_fixed(char dest[8], uint32_t number) {
    dest[0] = hex_digits[(number >> 28) & 0xf];
    dest[1] = hex_digits[(number >> 24) & 0xf];
    dest[2] = hex_digits[(number >> 20) & 0xf];
    dest[3] = hex_digits[(number >> 16) & 0xf];
    dest[4] = hex_digits[(number >> 12) & 0xf];
    dest[5] = hex_digits[(number >> 8) & 0xf];
    dest[6] = hex_digits[(number >> 4) & 0xf];
    dest[7] = hex_digits[number & 0xf];
}

static gcc_always_inline void
format_uint64_hex_fixed(char dest[16], uint64_t number) {
    format_uint32_hex_fixed(dest, number >> 32);
    format_uint32_hex_fixed(dest + 8, number);
}

/**
 * Format a 32 bit unsigned integer into a hex string.
 */
static gcc_always_inline size_t
format_uint32_hex(char dest[9], uint32_t number)
{
    char *p = dest + 9 - 1;

    *p = 0;
    do {
        --p;
        *p = hex_digits[number % 0x10];
        number /= 0x10;
    } while (number != 0);

    if (p > dest)
        memmove(dest, p, dest + 9 - p);

    return dest + 9 - p - 1;
}

#endif
