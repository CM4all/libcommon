// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <sys/resource.h>

#include <array>
#include <cstddef>
#include <string_view>

struct ResourceLimit : rlimit {
	static constexpr rlim_t UNDEFINED = rlim_t(-2);

	constexpr ResourceLimit() noexcept
		:rlimit{UNDEFINED, UNDEFINED} {}

	constexpr bool IsEmpty() const noexcept {
		return rlim_cur == UNDEFINED && rlim_max == UNDEFINED;
	}

	constexpr bool IsFull() const noexcept {
		return rlim_cur != UNDEFINED && rlim_max != UNDEFINED;
	}

	constexpr bool IsHigherThan(const ResourceLimit &other) const noexcept {
		return (rlim_cur != UNDEFINED && rlim_cur > other.rlim_cur) ||
			(rlim_max != UNDEFINED && rlim_max > other.rlim_max);
	}

	/**
	 * Throws std::system_error on error.
	 */
	void Load(int pid, int resource);

	/**
	 * Throws std::system_error on error.
	 */
	void Apply(int pid, int resource) const;

	void OverrideFrom(const ResourceLimit &src) noexcept;

	/**
	 * Throws std::system_error on error.
	 */
	void CompleteFrom(int pid, int resource, const ResourceLimit &src);
};

/**
 * Resource limits.
 */
struct ResourceLimits {
	std::array<ResourceLimit, RLIM_NLIMITS> values;

	[[gnu::pure]]
	bool IsEmpty() const noexcept;

	[[gnu::pure]]
	std::size_t GetHash() const noexcept;

	char *MakeId(char *p) const noexcept;

	/**
	 * Throws std::system_error on error.
	 */
	void Apply(int pid) const;

	/**
	 * Like Apply(), but only apply those limits that are higher
	 * than the current set of limits (i.e. those that require
	 * CAP_SYS_RESOURCE).
	 */
	void ApplyRaise(int pid, const ResourceLimits &current) const;

	/**
	 * Like Apply(), but only apply those limits that are not
	 * higher than the current set of limits (i.e. those that do
	 * not require CAP_SYS_RESOURCE).
	 */
	void ApplyLower(int pid, const ResourceLimits &current) const;

	bool Parse(std::string_view s) noexcept;
};
