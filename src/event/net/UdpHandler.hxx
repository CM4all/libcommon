// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <exception>
#include <span>

class SocketAddress;
class UniqueFileDescriptor;

/**
 * Handler for a #UdpListener.
 *
 * This is a class for a smooth API transition away from #UdpHandler
 * to an interface which allows receiving file descriptors.
 */
class UdpHandler {
public:
	/**
	 * Exceptions thrown by this method will be passed to OnUdpError().
	 *
	 * @param uid the peer process uid, or -1 if unknown
	 * @return false if the #UdpHandler was destroyed inside this method
	 */
	virtual bool OnUdpDatagram(std::span<const std::byte> payload,
				   std::span<UniqueFileDescriptor> fds,
				   SocketAddress address, int uid) = 0;

	/**
	 * The peer has hung up the (SOCK_SEQPACKET) connection.  This
	 * method can be used to throw an exception.
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
