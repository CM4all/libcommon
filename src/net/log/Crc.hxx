// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/CRC32.hxx"

namespace Net::Log {

/**
 * The CRC algorithm.
 */
using Crc = CRC32State;

} // namespace Net::Log
