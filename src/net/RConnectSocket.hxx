/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
