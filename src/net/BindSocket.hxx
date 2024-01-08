// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

class UniqueSocketDescriptor;
class SocketAddress;
class AddressInfo;

/**
 * Create a socket and bind it; a shortcut for
 * UniqueSocketDescriptor::Create() and
 * UniqueSocketDescriptor::Bind().
 *
 * Throws on error.
 */
UniqueSocketDescriptor
BindSocket(int domain, int type, int protocol, SocketAddress address);

UniqueSocketDescriptor
BindSocket(int type, SocketAddress address);

UniqueSocketDescriptor
BindSocket(const AddressInfo &ai);

/**
 * Create a socket bound to a port on the loopback interface.
 */
UniqueSocketDescriptor
BindLoopback(int type, unsigned port);
