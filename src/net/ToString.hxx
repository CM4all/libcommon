/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef NET_TO_STRING_HXX
#define NET_TO_STRING_HXX

#include <stddef.h>

class SocketAddress;

/**
 * Generates the string representation of a #SocketAddress into the
 * specified buffer.
 *
 * @return true on success
 */
bool
ToString(char *buffer, size_t buffer_size, SocketAddress address);

/**
 * Generates the string representation of a #SocketAddress into the
 * specified buffer, without the port number.
 *
 * @return true on success
 */
bool
HostToString(char *buffer, size_t buffer_size, SocketAddress address);

#endif
