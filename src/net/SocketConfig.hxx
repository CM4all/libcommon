// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "AllocatedSocketAddress.hxx"

#include <cstdint>
#include <string>
#include <utility>

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

	/**
	 * Enable Multi-Path TCP?
	 */
	bool mptcp = false;

	bool reuse_port = false;

	bool free_bind = false;

	bool pass_cred = false;

	/**
	 * If true, then disable Nagle's algorithm.
	 *
	 * @see TCP_NODELAY
	 */
	bool tcp_no_delay = false;

	/**
	 * @see SO_KEEPALIVE
	 */
	bool keepalive = false;

	/**
	 * @see IPV6_V6ONLY
	 */
	bool v6only = false;

	/**
	 * Apply fixups after configuration:
	 *
	 * - if bind_address is IPv6-wildcard, but multicast_group is
	 *   IPv4, then change bind_address to IPv4-wildcard
	 */
	void Fixup() noexcept;

	/**
	 * Create a listening socket.
	 *
	 * Throws exception on error.
	 */
	UniqueSocketDescriptor Create(int type) const;
};
