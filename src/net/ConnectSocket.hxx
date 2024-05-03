// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

class SocketAddress;
class UniqueSocketDescriptor;

/**
 * Create a (blocking) socket and connect it.
 *
 * Throws on error.
 */
UniqueSocketDescriptor
CreateConnectSocket(SocketAddress address, int type);

/**
 * Create a non-blocking socket and connect it.
 *
 * Throws on error.
 */
UniqueSocketDescriptor
CreateConnectSocketNonBlock(SocketAddress address, int type);

/**
 * Create a non-blocking datagram socket and connect it.
 *
 * Throws on error.
 */
UniqueSocketDescriptor
CreateConnectDatagramSocket(const SocketAddress address);
