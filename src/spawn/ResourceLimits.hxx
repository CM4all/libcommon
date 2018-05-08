/*
 * Copyright 2007-2017 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
