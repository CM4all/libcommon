// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <chrono>

template<size_t> class StringBuffer;

namespace Pg {

std::chrono::system_clock::time_point
ParseTimestamp(const char *s);

/**
 * Format the given time_point as a PostgreSQL timestamp without time
 * zone.
 */
StringBuffer<64>
FormatTimestamp(std::chrono::system_clock::time_point tp) noexcept;

}
