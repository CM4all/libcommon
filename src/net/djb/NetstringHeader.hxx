/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef NETSTRING_HEADER_HXX
#define NETSTRING_HEADER_HXX

#include <stddef.h>

struct StringView;

class NetstringHeader {
    char buffer[32];

public:
    StringView operator()(size_t size);
};

#endif
