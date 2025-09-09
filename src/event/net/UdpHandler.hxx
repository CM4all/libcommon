// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <exception>
#include <span>

class SocketAddress;
class UniqueFileDescriptor;

/**
 * Handler for #UdpListener and #MultiUdpListener.
 */
class UdpHandler {
public:
	/**
	 * A datagram was received.
	 * 
	 * Exceptions thrown by this method will be passed to OnUdpError().
	 *
	 * @param uid the peer process uid, or -1 if unknown
	 * @return false if the #UdpHandler was destroyed inside this method
	 */
	virtual bool OnUdpDatagram(std::span<const std::byte> payload,
				   std::span<UniqueFileDescriptor> fds,
				   SocketAddress address, int uid) = 0;

	/**
	 * The peer has hung up the (SOCK_SEQPACKET) connection.  The
	 * implementation has three choices:
	 *
	 * 1. return true and handle packets that may remain in the
	 *    receive queue
	 * 2. delete the connection and return false
	 * 3. throw exception (will be passed to OnUdpError())
	 *
	 * @return false if the #UdpHandler was destroyed inside this method
	 */
	virtual bool OnUdpHangup() {
		return true;
	}

	/**
	 * An I/O error has occurred, and the socket is defunct.
	 * After returning, it is assumed that the #UdpListener has
	 * been destroyed.
	 */
	virtual void OnUdpError(std::exception_ptr ep) noexcept = 0;
};
