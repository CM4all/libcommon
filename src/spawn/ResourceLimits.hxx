// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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

	/**
	 * Throws std::system_error on error.
	 */
	void Get(int pid, int resource);

	/**
	 * Throws std::system_error on error.
	 */
	void Set(int pid, int resource) const;

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

	bool Parse(std::string_view s) noexcept;
};
