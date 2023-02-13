// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "UniqueFileDescriptor.hxx"

/**
 * Remember the current working directory, optionally switch to a
 * different one, and restore the old working directory at the end of
 * the scope (in the destructor).
 */
class ScopeChdir {
	const UniqueFileDescriptor old;

public:
	/**
	 * Remember the current working directory, but do not change
	 * it now.
	 */
	[[nodiscard]]
	ScopeChdir();

	/**
	 * Change to the specified directory.
	 */
	[[nodiscard]]
	explicit ScopeChdir(const char *path);

	/**
	 * Change to the specified directory.
	 */
	[[nodiscard]]
	explicit ScopeChdir(FileDescriptor new_wd);

	~ScopeChdir() noexcept;

	ScopeChdir(const ScopeChdir &) = delete;
	ScopeChdir &operator=(const ScopeChdir &) = delete;
};
