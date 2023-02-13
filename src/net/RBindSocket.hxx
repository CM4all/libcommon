// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef RESOLVE_BIND_SOCKET_HXX
#define RESOLVE_BIND_SOCKET_HXX

struct addrinfo;
class UniqueSocketDescriptor;

/**
 * Resolve a host name and bind to the first resulting address.
 *
 * Throws std::runtime_error on error.
 *
 * @return the socket (non-blocking)
 */
UniqueSocketDescriptor
ResolveBindSocket(const char *host_and_port, int default_port,
		  const struct addrinfo &hints);

/**
 * A specialization of ResolveBindSocket() for SOCK_STREAM and support
 * for local sockets.
 */
UniqueSocketDescriptor
ResolveBindStreamSocket(const char *host_and_port, int default_port);

/**
 * A specialization of ResolveBindSocket() for SOCK_DGRAM and support
 * for local sockets.
 */
UniqueSocketDescriptor
ResolveBindDatagramSocket(const char *host_and_port, int default_port);

#endif
