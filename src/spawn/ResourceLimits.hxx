// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef BENG_PROXY_RESOURCE_LIMITS_HXX
#define BENG_PROXY_RESOURCE_LIMITS_HXX

#include <sys/resource.h>

struct ResourceLimit : rlimit {
	static constexpr rlim_t UNDEFINED = rlim_t(-2);

	constexpr ResourceLimit()
	:rlimit{UNDEFINED, UNDEFINED} {}

	constexpr bool IsEmpty() const {
		return rlim_cur == UNDEFINED && rlim_max == UNDEFINED;
	}

	constexpr bool IsFull() const {
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

	void OverrideFrom(const ResourceLimit &src);

	/**
	 * Throws std::system_error on error.
	 */
	void CompleteFrom(int pid, int resource, const ResourceLimit &src);
};

/**
 * Resource limits.
 */
struct ResourceLimits {
	ResourceLimit values[RLIM_NLIMITS];

	bool IsEmpty() const;

	unsigned GetHash() const;

	char *MakeId(char *p) const;

	/**
	 * Throws std::system_error on error.
	 */
	void Apply(int pid) const;

	bool Parse(const char *s);
};

#endif
