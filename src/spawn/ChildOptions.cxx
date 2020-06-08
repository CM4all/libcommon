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

#include "ChildOptions.hxx"
#include "ResourceLimits.hxx"
#include "Prepared.hxx"
#include "AllocatorPtr.hxx"
#include "system/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/djbhash.h"

#if TRANSLATION_ENABLE_EXPAND
#include "pexpand.hxx"
#endif

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

ChildOptions::ChildOptions(AllocatorPtr alloc,
			   const ChildOptions &src)
	:tag(alloc.CheckDup(src.tag)),
	 stderr_path(alloc.CheckDup(src.stderr_path)),
	 expand_stderr_path(alloc.CheckDup(src.expand_stderr_path)),
	 env(alloc, src.env),
	 cgroup(alloc, src.cgroup),
	 rlimits(src.rlimits != nullptr
		 ? alloc.New<ResourceLimits>(*src.rlimits)
		 : nullptr),
	 ns(alloc, src.ns),
	 uid_gid(src.uid_gid),
	 umask(src.umask),
	 stderr_null(src.stderr_null),
	 stderr_jailed(src.stderr_jailed),
	 stderr_pond(src.stderr_pond),
	 forbid_user_ns(src.forbid_user_ns),
	 forbid_multicast(src.forbid_multicast),
	 forbid_bind(src.forbid_bind),
	 no_new_privs(src.no_new_privs)
{
}

void
ChildOptions::Check() const
{
}

#if TRANSLATION_ENABLE_EXPAND

bool
ChildOptions::IsExpandable() const
{
	return expand_stderr_path != nullptr ||
		env.IsExpandable() ||
		ns.IsExpandable();
}

void
ChildOptions::Expand(AllocatorPtr alloc, const MatchInfo &match_info)
{
	if (expand_stderr_path != nullptr)
		stderr_path = expand_string_unescaped(alloc, expand_stderr_path,
						      match_info);

	env.Expand(alloc, match_info);
	ns.Expand(alloc, match_info);
}

#endif

char *
ChildOptions::MakeId(char *p) const
{
	if (umask >= 0)
		p += sprintf(p, ";u%o", umask);

	if (stderr_path != nullptr)
		p += sprintf(p, ";e%08x", djb_hash_string(stderr_path));

	if (stderr_jailed)
		*p++ = 'j';

	for (auto i : env) {
		*p++ = '$';
		p = stpcpy(p, i);
	}

	p = cgroup.MakeId(p);
	if (rlimits != nullptr)
		p = rlimits->MakeId(p);
	p = ns.MakeId(p);
	p = uid_gid.MakeId(p);

	if (stderr_null) {
		*p++ = ';';
		*p++ = 'e';
		*p++ = 'n';
	}

	if (forbid_user_ns) {
		*p++ = ';';
		*p++ = 'f';
		*p++ = 'u';
	}

	if (forbid_multicast) {
		*p++ = ';';
		*p++ = 'f';
		*p++ = 'm';
	}

	if (forbid_bind) {
		*p++ = ';';
		*p++ = 'f';
		*p++ = 'b';
	}

	if (no_new_privs) {
		*p++ = ';';
		*p++ = 'n';
	}

	return p;
}

UniqueFileDescriptor
ChildOptions::OpenStderrPath() const
{
	assert(stderr_path != nullptr);

	UniqueFileDescriptor fd;
	if (!fd.Open(stderr_path, O_CREAT|O_WRONLY|O_APPEND, 0600))
		throw FormatErrno("open('%s') failed", stderr_path);

	return fd;
}

void
ChildOptions::CopyTo(PreparedChildProcess &dest) const
{
	dest.umask = umask;

	if (stderr_jailed) {
		assert(stderr_path != nullptr);

		/* open the file in the child process after jailing */
		dest.stderr_path = stderr_path;
	} else if (stderr_path != nullptr) {
		dest.SetStderr(OpenStderrPath());
	} else if (stderr_null) {
		const char *path = "/dev/null";
		int fd = open(path, O_WRONLY|O_CLOEXEC|O_NOCTTY);
		if (fd >= 0)
			dest.SetStderr(fd);
	}

	for (const char *e : env)
		dest.PutEnv(e);

	dest.cgroup = &cgroup;
	dest.ns = ns;
	if (rlimits != nullptr)
		dest.rlimits = *rlimits;
	dest.uid_gid = uid_gid;
	dest.forbid_user_ns = forbid_user_ns;
	dest.forbid_multicast = forbid_multicast;
	dest.forbid_bind = forbid_bind;
	dest.no_new_privs = no_new_privs;

	if (!forbid_user_ns)
		/* if we allow user namespaces, then we should allow writing
		   to /proc/self/{uid,gid}_map, which requires a /proc mount
		   which is not read-only */
		dest.ns.mount.writable_proc = true;
}
