// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <span>

namespace Net::Log {

struct Datagram;

class BufferTooSmall {};

/**
 * Serialize the data of a #Datagram instance into a buffer (including
 * the protocol "magic" and CRC).
 *
 * May throw #BufferTooSmall if the given buffer is too small to hold
 * the whole datagram.
 *
 * @return the actual size of the buffer
 */
std::size_t
Serialize(std::span<std::byte> dest, const Datagram &d);

} // namespace Net::Log
