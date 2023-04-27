// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Mount.hxx"
#include "Error.hxx"

#include <sys/mount.h>
#include <errno.h>

void
MountOrThrow(const char *source, const char *target,
	     const char *filesystemtype, unsigned long mountflags,
	     const void *data)
{
	if (mount(source, target, filesystemtype, mountflags, data) < 0)
		throw FormatErrno("mount('%s') failed", target);
}

void
BindMount(const char *source, const char *target)
{
	if (mount(source, target, nullptr, MS_BIND, nullptr) < 0)
		throw FormatErrno("bind_mount('%s', '%s') failed", source, target);
}

void
BindMount(const char *source, const char *target, unsigned long flags)
{
	BindMount(source, target);

	/* wish we could just pass additional flags to the first mount
	   call, but unfortunately that doesn't work, the kernel ignores
	   these flags */
	if (flags != 0 &&
	    mount(nullptr, target, nullptr, MS_REMOUNT|MS_BIND|flags,
		  nullptr) < 0 &&

	    /* after EPERM, try again with MS_NOEXEC just in case this
	       missing flag was the reason for the kernel to reject our
	       request */
	    (errno != EPERM ||
	     (flags & MS_NOEXEC) != 0 ||
	     mount(nullptr, target, nullptr, MS_REMOUNT|MS_BIND|MS_NOEXEC|flags,
		   nullptr) < 0))
		throw FormatErrno("remount('%s') failed", target);
}
