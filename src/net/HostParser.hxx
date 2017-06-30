/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef NET_HOST_PARSER_HXX
#define NET_HOST_PARSER_HXX

#include "util/StringView.hxx"

#include "util/Compiler.h"

/**
 * Result type for ExtractHost().
 */
struct ExtractHostResult {
    /**
     * The host part of the address.
     *
     * If nothing was parsed, then this is nullptr.
     */
    StringView host;

    /**
     * Pointer to the first character that was not parsed.  On
     * success, this is usually a pointer to the zero terminator or to
     * a colon followed by a port number.
     *
     * If nothing was parsed, then this is a pointer to the given
     * source string.
     */
    const char *end;

    constexpr bool HasFailed() const {
        return host.IsNull();
    }
};

/**
 * Extract the host from a string in the form "IP:PORT" or
 * "[IPv6]:PORT".  Stops at the first invalid character (e.g. the
 * colon).
 *
 * @param src the input string
 */
gcc_pure
ExtractHostResult
ExtractHost(const char *src);

#endif
