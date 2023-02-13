// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <chrono>

#include <stdint.h>

namespace Net {
namespace Log {

using Duration = std::chrono::duration<uint64_t, std::micro>;
using TimePoint = std::chrono::time_point<std::chrono::system_clock, Duration>;

constexpr TimePoint
FromSystem(std::chrono::system_clock::time_point t) noexcept
{
	using namespace std::chrono;
	return TimePoint(duration_cast<Duration>(t.time_since_epoch()));
}

constexpr std::chrono::system_clock::time_point
ToSystem(TimePoint t) noexcept
{
	using namespace std::chrono;
	return system_clock::time_point(duration_cast<system_clock::duration>(t.time_since_epoch()));
}

}}
