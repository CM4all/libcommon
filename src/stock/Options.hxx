// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/Chrono.hxx"

#include <cstddef>

/**
 * Options for class #BasicStock.
 */
struct BasicStockOptions {
	std::size_t max_idle;

	Event::Duration clear_interval;
};

/**
 * Options for class #Stock.
 */
struct StockOptions {
	std::size_t limit;
	std::size_t max_idle;
	Event::Duration clear_interval;

	constexpr operator BasicStockOptions() const noexcept {
		return {
			.max_idle = max_idle,
			.clear_interval = clear_interval,
		};
	}
};
