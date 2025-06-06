// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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
	 * BIND_FILE, NAMED_TMPFS.
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
		 * Mount the tmpfs with the gviven name (#source) on
		 * #target.  If a tmpfs with that name does not exist,
		 * an empty one is created and will remain for some
		 * time even after the last child process using it
		 * exits.
		 */
		NAMED_TMPFS,

		/**
		 * Write #source to the read-only file #target.  This
		 * either creates a new file in tmpfs (if #target is
		 * located in a tmpfs) or bind-mounts a tmpfs file to
		 * the given #target (which must already exist as a
		 * regular file).
		 */
		WRITE_FILE,

		/**
		 * Create a symlink.  #source is the symlink target,
		 * #target is the link path.  Sorry for the confusing
		 * nomenclature!
		 */
		SYMLINK,
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

	struct NamedTmpfs {};

	constexpr Mount(NamedTmpfs, const char *_name, const char *_target,
			bool _writable) noexcept
		:source(_name), target(_target),
		 type(Type::NAMED_TMPFS),
		 writable(_writable), exec(false) {}

	struct WriteFile {};

	constexpr Mount(WriteFile, const char *path,
			const char *contents) noexcept
		:source(contents), target(path),
		 type(Type::WRITE_FILE),
		 writable(false), exec(false) {}

	Mount(AllocatorPtr alloc, const Mount &src) noexcept;

	/**
	 * Compare the source path with the specified one and return
	 * true if they are equal.
	 *
	 * @param path an absolute path
	 */
	[[gnu::pure]]
	bool IsSourcePath(const char *path) const noexcept;

	/**
	 * Check if the source path is equal or "above" the specified
	 * path.  If both paths are equal, returns an empty string.
	 * If the specified path is below the source path, returns the
	 * remaining string (starting with a slash).  Returns nullptr
	 * on mismatch.
	 */
	[[gnu::pure]]
	const char *IsInSourcePath(const char *path) const noexcept;

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
	void ApplyNamedTmpfs(VfsBuilder &vfs_builder) const;
	void ApplyWriteFile(VfsBuilder &vfs_builder) const;
	void ApplySymlink(VfsBuilder &vfs_builder) const;

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
