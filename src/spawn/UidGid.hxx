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

#ifndef BENG_PROXY_UID_GID_HXX
#define BENG_PROXY_UID_GID_HXX

#include <array>
#include <algorithm>

#include <sys/types.h>

struct UidGid {
	uid_t uid;
	gid_t gid;

	std::array<gid_t, 32> groups;

	constexpr UidGid() noexcept:uid(0), gid(0), groups{{0}} {}

	/**
	 * Look up a user name in the system user database (/etc/passwd)
	 * and fill #uid, #gid and #groups.
	 *
	 * Throws std::runtime_error on error.
	 */
	void Lookup(const char *username);

	void LoadEffective() noexcept;

	constexpr bool IsEmpty() const noexcept {
		return uid == 0 && gid == 0 && !HasGroups();
	}

	constexpr bool IsComplete() const noexcept {
		return uid != 0 && gid != 0;
	}

	bool HasGroups() const noexcept {
		return groups.front() != 0;
	}

	size_t CountGroups() const noexcept {
		return std::distance(groups.begin(),
				     std::find(groups.begin(), groups.end(), 0));
	}

	char *MakeId(char *p) const noexcept;

	/**
	 * Throws std::system_error on error.
	 */
	void Apply() const;
};

#endif
