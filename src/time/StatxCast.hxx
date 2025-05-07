// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <chrono>

#include <sys/stat.h>

/**
 * Cast a #statx_timestamp (returned from the Linux statx() system
 * call) to a std::chrono::system_clock::time_point.
 */
constexpr std::chrono::system_clock::time_point
ToSystemTimePoint(const struct statx_timestamp ts) noexcept
{
	return std::chrono::system_clock::from_time_t(ts.tv_sec)
		+ std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::nanoseconds(ts.tv_nsec));
}
