// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "io/FileDescriptor.hxx"
#include "util/TagStructs.hxx"

#include <cstdint>

class AllocatorPtr;

struct PidNamespaceOptions {
	/**
	 * The name of the PID namespace to reassociate with.  The
	 * namespace is requested from the "Spawn-Accessory" daemon
	 * (Package cm4all-spawn-accessory).
	 */
	const char *name = nullptr;

	/**
	 * If this field is set, then reassociate with this PID
	 * namespace.
	 *
	 * The file descriptor is owned by the caller.
	 */
	FileDescriptor fd = FileDescriptor::Undefined();

	/**
	 * Start the child process in a new PID namespace?
	 */
	enum class Mode : uint_least8_t {
		/**
		 * No, run the child process in the same PID namespace
		 * as this process.
		 */
		DISABLED,

		/**
		 * Create a new anonymous PID namespace.
		 */
		ANONYMOUS,

		/**
		 * Reassociate with a PID namespace managed by the
		 * cm4all-spawn-accessory daemon.  Uses the #name
		 * field.
		 */
		ACCESSORY,
	} mode = Mode::DISABLED;

	PidNamespaceOptions() noexcept = default;

	constexpr PidNamespaceOptions([[maybe_unused]] ShallowCopy shallow_copy,
				      const PidNamespaceOptions &src) noexcept
		:name(src.name),
		 fd(src.fd),
		 mode(src.mode)
	{
	}

	PidNamespaceOptions(AllocatorPtr alloc, const PidNamespaceOptions &src) noexcept;

	[[gnu::pure]]
	uint_least64_t GetCloneFlags(uint_least64_t flags) const noexcept;

	char *MakeId(char *p) const noexcept;
};
