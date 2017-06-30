/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef HTTP_RANGE_HXX
#define HTTP_RANGE_HXX

#include "util/Compiler.h"

#include <chrono>

struct HttpRangeRequest {
    enum class Type {
        NONE,
        VALID,
        INVALID,
    } type = Type::NONE;

    uint64_t skip = 0, size;

    explicit HttpRangeRequest(uint64_t _size):size(_size) {}

    /**
     * Parse a "Range" request header.
     */
    void ParseRangeHeader(const char *p);
};

#endif
