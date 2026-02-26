// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/TagStructs.hxx"

#include <cstdint>

class AllocatorPtr;

struct PidNamespaceOptions {
	/**
	 * The name of the PID namespace to reassociate with.  The
	 * namespace is requested from the "Spawn" daemon (Package
	 * cm4all-spawn).
	 */
	const char *name = nullptr;

	/**
	 * Start the child process in a new PID namespace?
	 */
	bool enable = false;

	PidNamespaceOptions() noexcept = default;

	constexpr PidNamespaceOptions([[maybe_unused]] ShallowCopy shallow_copy,
				      const PidNamespaceOptions &src) noexcept
		:name(src.name),
		 enable(src.enable)
	{
	}

	PidNamespaceOptions(AllocatorPtr alloc, const PidNamespaceOptions &src) noexcept;

	[[gnu::pure]]
	uint_least64_t GetCloneFlags(uint_least64_t flags) const noexcept;

	char *MakeId(char *p) const noexcept;
};
