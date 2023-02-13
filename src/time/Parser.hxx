// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef TIME_PARSER_HXX
#define TIME_PARSER_HXX

#include <chrono>
#include <utility>

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

#endif
