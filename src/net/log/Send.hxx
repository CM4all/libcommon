// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

class SocketDescriptor;

namespace Net::Log {

struct Datagram;

/**
 * Send a log datagram on the given socket.
 *
 * Throws on error.
 */
void
Send(SocketDescriptor s, const Datagram &d);

} // namespace Net::Log
