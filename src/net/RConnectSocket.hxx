// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef RESOLVE_CONNECT_SOCKET_HXX
#define RESOLVE_CONNECT_SOCKET_HXX

#include <chrono>

struct addrinfo;
class UniqueSocketDescriptor;

/**
 * Resolve a host name and connect to the first resulting address
 * (synchronously).
 *
 * Throws std::runtime_error on error.
 *
 * @return the connected socket (non-blocking)
 */
UniqueSocketDescriptor
ResolveConnectSocket(const char *host_and_port, int default_port,
		     const struct addrinfo &hints,
		     std::chrono::duration<int, std::milli> timeout=std::chrono::seconds(60));

UniqueSocketDescriptor
ResolveConnectStreamSocket(const char *host_and_port, int default_port,
			   std::chrono::duration<int, std::milli> timeout=std::chrono::seconds(60));

UniqueSocketDescriptor
ResolveConnectDatagramSocket(const char *host_and_port, int default_port);

#endif
