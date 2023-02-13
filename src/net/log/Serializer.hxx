// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>

namespace Net {
namespace Log {

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
Serialize(void *buffer, std::size_t size, const Datagram &d);

}}
