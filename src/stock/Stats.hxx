// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "event/Chrono.hxx"

#include <cstddef>

struct StockCounters {
	/**
	 * Number of items that were attempted to be created, were
	 * canceled, were successful and failed.
	 */
	std::size_t total_creates, canceled_creates, successful_creates, failed_creates;

	std::size_t total_waits, canceled_waits, successful_waits, failed_waits;

	/**
	 * Total wait time.
	 */
	Event::Duration total_wait_duration;

	std::size_t rejects;

	constexpr auto &operator+=(const StockCounters &other) noexcept {
		total_creates += other.total_creates;
		canceled_creates += other.canceled_creates;
		successful_creates += other.successful_creates;
		failed_creates += other.failed_creates;
		total_waits += other.total_waits;
		canceled_waits += other.canceled_waits;
		successful_waits += other.successful_waits;
		failed_waits += other.failed_waits;
		total_wait_duration += other.total_wait_duration;
		rejects += other.rejects;
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
