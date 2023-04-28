// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "translation/Features.hxx"
#include "io/FileDescriptor.hxx"
#include "util/IntrusiveForwardList.hxx"

#include <cstdint>

class AllocatorPtr;
class MatchData;
class VfsBuilder;

struct Mount : IntrusiveForwardListHook {
	const char *source;
	const char *target;

	/**
	 * If this is defined, then it is used instead of #source.
	 * This is useful for instances inside #PreparedChildProcess
	 * where the caller may want to prepare mounting.  The file
	 * descriptor is owned by the caller.
	 *
	 * This is only supported by the following types: BIND,
	 * BIND_FILE.
	 */
	FileDescriptor source_fd = FileDescriptor::Undefined();

	enum class Type : uint_least8_t {
		/**
		 * Bind-mount the directory #source onto #target.
		 */
		BIND,

		/**
		 * Bind-mount the file #source onto #target.
		 */
		BIND_FILE,

		/**
		 * Mount an empty tmpfs on #target.
		 */
		TMPFS,

		/**
		 * Write #source to the read-only file #target.  This
		 * either creates a new file in tmpfs (if #target is
		 * located in a tmpfs) or bind-mounts a tmpfs file to
		 * the given #target (which must already exist as a
		 * regular file).
		 */
		WRITE_FILE,
	} type = Type::BIND;

#if TRANSLATION_ENABLE_EXPAND
	bool expand_source = false;
#endif

	bool writable;

	/**
	 * Omit the MS_NOEXEC flag?
	 */
	bool exec;

	/**
	 * Ignore ENOENT?
	 */
	bool optional = false;

	constexpr Mount(const char *_source, const char *_target,
			bool _writable=false,
			bool _exec=false) noexcept
		:source(_source), target(_target),
		writable(_writable), exec(_exec) {
	}

	struct Tmpfs {};

	constexpr Mount(Tmpfs, const char *_target, bool _writable) noexcept
		:source(nullptr), target(_target),
		 type(Type::TMPFS),
		 writable(_writable), exec(false) {}

	struct WriteFile {};

	constexpr Mount(WriteFile, const char *path,
			const char *contents) noexcept
		:source(contents), target(path),
		 type(Type::WRITE_FILE),
		 writable(false), exec(false) {}

	Mount(AllocatorPtr alloc, const Mount &src) noexcept;

#if TRANSLATION_ENABLE_EXPAND
	bool IsExpandable() const noexcept {
		return expand_source;
	}

	[[gnu::pure]]
	static bool IsAnyExpandable(const IntrusiveForwardList<Mount> &list) noexcept {
		for (const auto &i : list)
			if (i.IsExpandable())
				return true;

		return false;
	}

	void Expand(AllocatorPtr alloc, const MatchData &match_data);
	static void ExpandAll(AllocatorPtr alloc,
			      IntrusiveForwardList<Mount> &list,
			      const MatchData &match_data);
#endif

private:
	void ApplyBindMount(VfsBuilder &vfs_builder) const;
	void ApplyBindMountFile(VfsBuilder &vfs_builder) const;
	void ApplyTmpfs(VfsBuilder &vfs_builder) const;
	void ApplyWriteFile(VfsBuilder &vfs_builder) const;

public:
	/**
	 * Throws std::system_error on error.
	 */
	void Apply(VfsBuilder &vfs_builder) const;

	static IntrusiveForwardList<Mount> CloneAll(AllocatorPtr alloc,
						    const IntrusiveForwardList<Mount> &src) noexcept;

	/**
	 * Throws std::system_error on error.
	 */
	static void ApplyAll(const IntrusiveForwardList<Mount> &m,
			     VfsBuilder &vfs_builder);

	char *MakeId(char *p) const noexcept;
	static char *MakeIdAll(char *p, const IntrusiveForwardList<Mount> &m) noexcept;
};
