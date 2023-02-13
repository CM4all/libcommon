// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <chrono>

namespace Pg {

/**
 * Parse a PostgreSQL "interval" string with second resolution.
 *
 * Throws std::invalid_argument on syntax error.
 */
std::chrono::seconds
ParseIntervalS(const char *s);

} /* namespace Pg */
