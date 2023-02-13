// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

class SocketDescriptor;

namespace Net {
namespace Log {

struct Datagram;

/**
 * Send a log datagram on the given socket.
 *
 * Throws on error.
 */
void
Send(SocketDescriptor s, const Datagram &d);

}}
