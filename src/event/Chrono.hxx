// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <chrono>

namespace Event {

/**
 * The clock used by classes #EventLoop, #CoarseTimerEvent and #FineTimerEvent.
 */
using Clock = std::chrono::steady_clock;

using Duration = Clock::duration;
using TimePoint = Clock::time_point;

} // namespace Event
