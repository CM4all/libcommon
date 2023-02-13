// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef> // for std::byte
#include <span>

namespace Net {
namespace Log {

struct Datagram;

class ProtocolError {};

/**
 * Throws #ProtocolError on error.
 */
Datagram
ParseDatagram(std::span<const std::byte> d);

}}
