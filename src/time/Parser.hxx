// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <chrono>
#include <utility>

/**
 * Parse a duration from a string, e.g. "30s", "5m", "3h", "7d".
 *
 * Throws on error.
 *
 * @return a pair consisting of the duration and the specified
 * precision
 */
std::pair<std::chrono::system_clock::duration,
	  std::chrono::system_clock::duration>
ParseDuration(const char *s);

/**
 * Parse a time stamp from a string.
 *
 * Throws on error.
 *
 * @return a pair consisting of the time point and the specified
 * precision; e.g. for a date, the second value is "one day"
 */
std::pair<std::chrono::system_clock::time_point,
	  std::chrono::system_clock::duration>
ParseTimePoint(const char *s);
