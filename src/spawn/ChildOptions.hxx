// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef BENG_PROXY_CHILD_OPTIONS_HXX
#define BENG_PROXY_CHILD_OPTIONS_HXX

#include "translation/Features.hxx"
#include "adata/ExpandableStringList.hxx"
#include "CgroupOptions.hxx"
#include "NamespaceOptions.hxx"
#include "UidGid.hxx"
#include "util/ShallowCopy.hxx"

#include <span>
#include <string_view>

struct ResourceLimits;
struct PreparedChildProcess;
class MatchData;
class UniqueFileDescriptor;
class FdHolder;

/**
 * Options for launching a child process.
 */
struct ChildOptions {
	/**
	 * A "tag" string for the child process.  This can be used to
	 * address groups of child processes.
	 *
	 * This field can contain multiple values separated by null
	 * bytes.
	 */
	std::string_view tag{};

	const char *chdir = nullptr;

	/**
	 * An absolute path where STDERR output will be appended.
	 */
	const char *stderr_path = nullptr;
	const char *expand_stderr_path = nullptr;

	/**
	 * Environment variables.
	 */
	ExpandableStringList env;

	CgroupOptions cgroup;

	ResourceLimits *rlimits = nullptr;

	NamespaceOptions ns;

	UidGid uid_gid;

	/**
	 * The umask for the new child process.  -1 means do not change
	 * it.
	 */
	int umask = -1;

	/**
	 * Redirect STDERR to /dev/null?
	 */
	bool stderr_null = false;

	/**
	 * Shall #stderr_path be applied after jailing?
	 *
	 * @see TranslationCommand::STDERR_PATH_JAILED
	 */
	bool stderr_jailed = false;

	/**
	 * Send the child's STDERR output to the configured Pond
	 * server instead of to systemd-journald.
	 *
	 * @see TranslationCommand::STDERR_POND
	 */
	bool stderr_pond = false;

	bool forbid_user_ns = false;

	bool forbid_multicast = false;

	bool forbid_bind = false;

	bool no_new_privs = false;

	ChildOptions() noexcept = default;

	constexpr ChildOptions(ShallowCopy shallow_copy,
			       const ChildOptions &src) noexcept
		:tag(src.tag),
		 chdir(src.chdir),
		 stderr_path(src.stderr_path),
		 expand_stderr_path(src.expand_stderr_path),
		 env(shallow_copy, src.env),
		 cgroup(shallow_copy, src.cgroup),
		 rlimits(src.rlimits),
		 ns(shallow_copy, src.ns),
		 uid_gid(src.uid_gid),
		 umask(src.umask),
		 stderr_null(src.stderr_null),
		 stderr_jailed(src.stderr_jailed),
		 stderr_pond(src.stderr_pond),
		 forbid_user_ns(src.forbid_user_ns),
		 forbid_multicast(src.forbid_multicast),
		 forbid_bind(src.forbid_bind),
		 no_new_privs(src.no_new_privs) {}

	ChildOptions(AllocatorPtr alloc, const ChildOptions &src) noexcept;

	ChildOptions(ChildOptions &&) noexcept = default;
	ChildOptions &operator=(ChildOptions &&) noexcept = default;

	/**
	 * Throws on error.
	 */
	void Check() const;

	[[gnu::pure]]
	bool IsExpandable() const noexcept;

	/**
	 * Throws on error.
	 */
	void Expand(AllocatorPtr alloc, const MatchData &match_data);

	char *MakeId(char *p) const noexcept;

	/**
	 * Throws on error.
	 */
	UniqueFileDescriptor OpenStderrPath() const;

	/**
	 * Throws error.
	 *
	 * @param close_fds a list of file descriptors that shall be
	 * closed after #dest has been destructed
	 */
	void CopyTo(PreparedChildProcess &dest, FdHolder &close_fds) const;
};

#endif
