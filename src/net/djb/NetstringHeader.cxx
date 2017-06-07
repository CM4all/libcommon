/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "NetstringHeader.hxx"
#include "util/StringView.hxx"

#include <stdio.h>

StringView
NetstringHeader::operator()(size_t size)
{
    return {buffer, (size_t)sprintf(buffer, "%zu:", size)};
}
