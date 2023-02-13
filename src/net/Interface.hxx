// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef NET_INTERFACE_HXX
#define NET_INTERFACE_HXX

class SocketAddress;

/**
 * Find a network interface with the given address.
 *
 * @return the interface index or 0 if no matching network interface
 * was found
 */
[[gnu::pure]]
unsigned
FindNetworkInterface(SocketAddress address) noexcept;

#endif
