// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef NET_TO_STRING_HXX
#define NET_TO_STRING_HXX

#include <cstddef>

class SocketAddress;

/**
 * Generates the string representation of a #SocketAddress into the
 * specified buffer.
 *
 * @return true on success
 */
bool
ToString(char *buffer, std::size_t buffer_size, SocketAddress address) noexcept;

/**
 * Like ToString() above, but return the string pointer (or on error:
 * return the given fallback pointer).
 */
const char *
ToString(char *buffer, std::size_t buffer_size, SocketAddress address,
	 const char *fallback) noexcept;

/**
 * Generates the string representation of a #SocketAddress into the
 * specified buffer, without the port number.
 *
 * @return true on success
 */
bool
HostToString(char *buffer, std::size_t buffer_size, SocketAddress address) noexcept;

#endif
