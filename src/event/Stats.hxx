// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Chrono.hxx"

#include <cstddef>

struct EventLoopStats {
	/**
	 * Total (wallclock) duration waiting for events (i.e. inside
	 * epoll_wait()).
	 */
	Event::Duration idle_duration{};

	/**
	 * Total (wallclock) duration handling events (i.e. busy).
	 */
	Event::Duration busy_duration{};

	/**
	 * Total number of iterations, i.e. number of epoll_wait()
	 * calls with a positive timeout parameter.
	 */
	uint_least64_t iterations = 0;
};
