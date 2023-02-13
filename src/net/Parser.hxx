// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef NET_PARSER_HXX
#define NET_PARSER_HXX

struct addrinfo;
class AllocatedSocketAddress;

/**
 * Parse a socket address or a local socket address (absolute
 * path starting with '/' or an abstract name starting with '@').
 *
 * Throws on error.
 */
AllocatedSocketAddress
ParseSocketAddress(const char *p, int default_port,
		   const struct addrinfo &hints);

/**
 * Parse a numeric socket address or a local socket address (absolute
 * path starting with '/' or an abstract name starting with '@').
 *
 * Throws std::runtime_error on error.
 */
AllocatedSocketAddress
ParseSocketAddress(const char *p, int default_port, bool passive);

#endif
