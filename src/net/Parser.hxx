// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

struct addrinfo;
class AllocatedSocketAddress;

/**
 * Parse a socket address or a local socket address (absolute
 * path starting with '/' or an abstract name starting with '@').
 *
 * Throws on error.  Resolver errors are thrown as std::system_error
 * with #resolver_error_category.
 */
AllocatedSocketAddress
ParseSocketAddress(const char *p, int default_port,
		   const struct addrinfo &hints);

/**
 * Parse a numeric socket address or a local socket address (absolute
 * path starting with '/' or an abstract name starting with '@').
 *
 * Throws on error.  Resolver errors are thrown as std::system_error
 * with #resolver_error_category.
 */
AllocatedSocketAddress
ParseSocketAddress(const char *p, int default_port, bool passive);
