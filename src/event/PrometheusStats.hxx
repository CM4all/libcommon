// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Stats.hxx"

#include <string>
#include <string_view>

#include <fmt/core.h>

inline std::string
ToPrometheusString(const EventLoopStats &stats, std::string_view process) noexcept
{
	using std::string_view_literals::operator""sv;

	return fmt::format(R"(
# HELP event_loop_iterations Total number of EventLoop iterations
# TYPE event_loop_iterations counter

# HELP event_loop_idle_duration Total duration waiting for events
# TYPE event_loop_idle_duration counter

# HELP event_loop_busy_duration Total duration handling events
# TYPE event_loop_busy_duration counter

event_loop_iterations{{process={:?}}} {}
event_loop_idle_duration{{process={:?}}} {}
event_loop_busy_duration{{process={:?}}} {}
)"sv,
			   process, stats.iterations,
			   process,
			   std::chrono::duration_cast<std::chrono::duration<double>>(stats.idle_duration).count(),
			   process,
			   std::chrono::duration_cast<std::chrono::duration<double>>(stats.busy_duration).count());
}
