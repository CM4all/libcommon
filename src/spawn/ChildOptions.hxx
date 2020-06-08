/*
 * Copyright 2007-2020 CM4all GmbH
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

#ifndef BENG_PROXY_CHILD_OPTIONS_HXX
#define BENG_PROXY_CHILD_OPTIONS_HXX

#include "translation/Features.hxx"
#include "adata/ExpandableStringList.hxx"
#include "CgroupOptions.hxx"
#include "NamespaceOptions.hxx"
#include "UidGid.hxx"
#include "util/ShallowCopy.hxx"

struct ResourceLimits;
struct PreparedChildProcess;
class MatchInfo;
class UniqueFileDescriptor;

/**
 * Options for launching a child process.
 */
struct ChildOptions {
	/**
	 * A "tag" string for the child process.  This can be used to
	 * address groups of child processes.
	 */
	const char *tag = nullptr;

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

	bool forbid_user_ns = false;

	bool forbid_multicast = false;

	bool forbid_bind = false;

	bool no_new_privs = false;

	ChildOptions() = default;

	constexpr ChildOptions(ShallowCopy shallow_copy, const ChildOptions &src)
		:tag(src.tag),
		stderr_path(src.stderr_path),
		expand_stderr_path(src.expand_stderr_path),
		env(shallow_copy, src.env),
		cgroup(shallow_copy, src.cgroup),
		rlimits(src.rlimits),
		ns(src.ns),
		uid_gid(src.uid_gid),
		umask(src.umask),
		stderr_null(src.stderr_null),
		stderr_jailed(src.stderr_jailed),
		forbid_user_ns(src.forbid_user_ns),
		forbid_multicast(src.forbid_multicast),
		forbid_bind(src.forbid_bind),
		no_new_privs(src.no_new_privs) {}

	ChildOptions(AllocatorPtr alloc, const ChildOptions &src);

	ChildOptions(ChildOptions &&) = default;
	ChildOptions &operator=(ChildOptions &&) = default;

	/**
	 * Throws std::runtime_error on error.
	 */
	void Check() const;

	gcc_pure
	bool IsExpandable() const;

	void Expand(AllocatorPtr alloc, const MatchInfo &match_info);

	char *MakeId(char *p) const;

	/**
	 * Throws on error.
	 */
	UniqueFileDescriptor OpenStderrPath() const;

	/**
	 * Throws std::runtime_error on error.
	 */
	void CopyTo(PreparedChildProcess &dest) const;
};

#endif
