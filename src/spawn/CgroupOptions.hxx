// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "adata/AssignmentList.hxx"
#include "util/TagStructs.hxx"

#include <string_view>

class AllocatorPtr;
class UniqueFileDescriptor;
struct CgroupState;

/**
 * Options for how to configure the cgroups of a process.
 */
struct CgroupOptions {
	/**
	 * The name of the cgroup this process will be moved into.  It
	 * is a name (without slashes) relative to the daemon's scope
	 * cgroup.  For example, it could be an identification of the
	 * hosting account which this process belongs to.
	 */
	const char *name = nullptr;

	/**
	 * A list of cgroup extended attributes.  They should usually
	 * be in the "user" namespace.
	 */
	AssignmentList xattr;

	/**
	 * A list of cgroup controller settings.
	 *
	 * The name of the controller setting,
	 * e.g. "cpu.shares".
	 *
	 * The value to be written to the specified setting.
	 */
	AssignmentList set;

	CgroupOptions() = default;
	CgroupOptions(AllocatorPtr alloc, const CgroupOptions &src) noexcept;

	constexpr CgroupOptions(ShallowCopy shallow_copy,
				const CgroupOptions &src) noexcept
		:name(src.name),
		 xattr(shallow_copy, src.xattr),
		 set(shallow_copy, src.set) {}

	CgroupOptions(CgroupOptions &&) = default;
	CgroupOptions &operator=(CgroupOptions &&) = default;

	constexpr bool IsDefined() const noexcept {
		return name != nullptr;
	}

	void SetXattr(AllocatorPtr alloc,
		      std::string_view name, std::string_view value) noexcept;

	void Set(AllocatorPtr alloc,
		 std::string_view name, std::string_view value) noexcept;

	/**
	 * Create a cgroup2 group.  Returns an undefined
	 * #UniqueFileDescriptor if this instance is not enabled.
	 *
	 * Throws on error.
	 *
	 * @param session if not nullptr, create one child cgroupbelow
	 * the one created by #name
	 */
	UniqueFileDescriptor Create2(const CgroupState &state,
				     const char *session) const;

	char *MakeId(char *p) const noexcept;
};
