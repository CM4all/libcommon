// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ChildOptions.hxx"
#include "ResourceLimits.hxx"
#include "Prepared.hxx"
#include "AllocatorPtr.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/FdHolder.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/Base32.hxx"
#include "util/djb_hash.hxx"

#if TRANSLATION_ENABLE_EXPAND
#include "pexpand.hxx"
#endif

#include <stdexcept>

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

ChildOptions::ChildOptions(AllocatorPtr alloc,
			   const ChildOptions &src) noexcept
	:tag(alloc.Dup(src.tag)),
	 chdir(alloc.CheckDup(src.chdir)),
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
#ifdef HAVE_LIBSECCOMP
	 forbid_user_ns(src.forbid_user_ns),
	 forbid_multicast(src.forbid_multicast),
	 forbid_bind(src.forbid_bind),
#endif // HAVE_LIBSECCOMP
#ifdef HAVE_LIBCAP
	 cap_sys_resource(src.cap_sys_resource),
#endif // HAVE_LIBCAP
	 no_new_privs(src.no_new_privs)
{
}

void
ChildOptions::Check() const
{
#ifdef HAVE_LIBCAP
	if (cap_sys_resource && ns.enable_user)
		throw std::runtime_error{"CAP_SYS_RESOURCE is not possible with USER_NAMESPACE"};
#endif
}

#if TRANSLATION_ENABLE_EXPAND

bool
ChildOptions::IsExpandable() const noexcept
{
	return expand_stderr_path != nullptr ||
		env.IsExpandable() ||
		ns.IsExpandable();
}

void
ChildOptions::Expand(AllocatorPtr alloc, const MatchData &match_data)
{
	if (expand_stderr_path != nullptr)
		stderr_path = expand_string_unescaped(alloc, expand_stderr_path,
						      match_data);

	env.Expand(alloc, match_data);
	ns.Expand(alloc, match_data);
}

#endif

char *
ChildOptions::MakeId(char *p) const noexcept
{
	if (umask >= 0)
		p += sprintf(p, ";u%o", umask);

	if (chdir != nullptr) {
		*p++ = ';';
		*p++ = 'c';
		*p++ = 'd';
		p = FormatIntBase32(p, djb_hash_string(chdir));
	}

	if (stderr_path != nullptr) {
		*p++ = ';';
		*p++ = 'e';
		p = FormatIntBase32(p, djb_hash_string(stderr_path));
	}

	if (stderr_jailed)
		*p++ = 'j';

	if (stderr_pond)
		*p++ = 'p';

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

#ifdef HAVE_LIBSECCOMP
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
#endif // HAVE_LIBSECCOMP

#ifdef HAVE_LIBCAP
	if (cap_sys_resource) {
		*p++ = ';';
		*p++ = 's';
		*p++ = 'r';
	}
#endif // HAVE_LIBCAP

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
		throw FmtErrno("open('{}') failed", stderr_path);

	return fd;
}

void
ChildOptions::CopyTo(PreparedChildProcess &dest, FdHolder &close_fds) const
{
	dest.umask = umask;

	dest.chroot = chroot;
	dest.chdir = chdir;

	if (stderr_jailed) {
		assert(stderr_path != nullptr);

		/* open the file in the child process after jailing */
		dest.stderr_path = stderr_path;
	} else if (stderr_path != nullptr) {
		dest.stderr_fd = close_fds.Insert(OpenStderrPath());
	} else if (stderr_null) {
		const char *path = "/dev/null";
		UniqueFileDescriptor fd;
		if (fd.Open(path, O_WRONLY))
			dest.stderr_fd = close_fds.Insert(std::move(fd));
	}

	for (const char *e : env)
		dest.PutEnv(e);

	dest.cgroup = &cgroup;
	dest.ns = {ShallowCopy{}, ns};
	if (rlimits != nullptr)
		dest.rlimits = *rlimits;
	dest.uid_gid = uid_gid;
#ifdef HAVE_LIBSECCOMP
	dest.forbid_user_ns = forbid_user_ns;
	dest.forbid_multicast = forbid_multicast;
	dest.forbid_bind = forbid_bind;
#endif // HAVE_LIBSECCOMP
#ifdef HAVE_LIBCAP
	dest.cap_sys_resource = cap_sys_resource;
#endif // HAVE_LIBCAP
	dest.no_new_privs = no_new_privs;

#ifdef HAVE_LIBSECCOMP
	if (!forbid_user_ns)
#endif
		/* if we allow user namespaces, then we should allow writing
		   to /proc/self/{uid,gid}_map, which requires a /proc mount
		   which is not read-only */
		dest.ns.mount.writable_proc = true;
}
