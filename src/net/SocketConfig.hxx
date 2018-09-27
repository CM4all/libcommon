/*
 * Copyright 2007-2017 Content Management AG
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

#pragma once

#include "AllocatedSocketAddress.hxx"

#include <string>

#include <stdint.h>

class UniqueSocketDescriptor;

struct SocketConfig {
	AllocatedSocketAddress bind_address;

	AllocatedSocketAddress multicast_group;

	/**
	 * If non-empty, sets SO_BINDTODEVICE.
	 */
	std::string interface;

	/**
	 * If non-zero, calls listen().  Value is the backlog.
	 */
	unsigned listen = 0;

	/**
	 * If non-zero, sets TCP_DEFER_ACCEPT.  Value is a number of
	 * seconds.
	 */
	unsigned tcp_defer_accept = 0;

	/**
	 * If non-zero, sets TCP_USER_TIMEOUT.  Value is a number of
	 * milliseconds.
	 */
	unsigned tcp_user_timeout = 0;

	/**
	 * If non-zero, sets the socket's file mode (overriding the
	 * umask).
	 */
	uint16_t mode = 0;

	bool reuse_port = false;

	bool free_bind = false;

	bool pass_cred = false;

	/**
	 * @see SO_KEEPALIVE
	 */
	bool keepalive = false;

	SocketConfig() = default;

	explicit SocketConfig(SocketAddress _bind_address)
		:bind_address(_bind_address) {}

	explicit SocketConfig(AllocatedSocketAddress &&_bind_address)
		:bind_address(std::move(_bind_address)) {}

	/**
	 * Apply fixups after configuration:
	 *
	 * - if bind_address is IPv6-wildcard, but multicast_group is
	 *   IPv4, then change bind_address to IPv4-wildcard
	 */
	void Fixup();

	/**
	 * Create a listening socket.
	 *
	 * Throws exception on error.
	 */
	UniqueSocketDescriptor Create(int type) const;
};
