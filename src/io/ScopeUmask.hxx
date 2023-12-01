// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <sys/stat.h>

/**
 * Set a new umask and restore the old one at the end of the scope.
 */
class ScopeUmask {
	const mode_t old_umask;

public:
	[[nodiscard]]
	explicit ScopeUmask(mode_t new_umask) noexcept
		:old_umask(umask(new_umask)) {}

	~ScopeUmask() noexcept {
		umask(old_umask);
	}

	ScopeUmask(const ScopeUmask &) = delete;
	ScopeUmask &operator=(const ScopeUmask &) = delete;
};
