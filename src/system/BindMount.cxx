/*
 * Copyright 2007-2017 Content Management AG
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

#include "BindMount.hxx"
#include "system/Error.hxx"

#include <sys/mount.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

void
BindMount(const char *source, const char *target, int flags)
{
    if (mount(source, target, nullptr, MS_BIND, nullptr) < 0)
        throw FormatErrno("bind_mount('%s', '%s') failed", source, target);

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
