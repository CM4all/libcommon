// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/Chrono.hxx"

#include <cstddef>

struct StockCounters {
	/**
	 * Total wait time.
	 */
	Event::Duration total_wait;

	constexpr auto &operator+=(const StockCounters &other) noexcept {
		total_wait += other.total_wait;
		return *this;
	}
};

struct StockStats : StockCounters {
	std::size_t busy, idle;

	/**
	 * Number of callers waiting for an item.
	 */
	std::size_t waiting;
};
