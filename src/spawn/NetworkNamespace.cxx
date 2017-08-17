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

#include "NetworkNamespace.hxx"
#include "system/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <sched.h>

/**
 * Open a network namespace in /run/netns.
 */
static UniqueFileDescriptor
OpenNetworkNS(const char *name)
{
    char path[4096];
    if (snprintf(path, sizeof(path),
                 "/run/netns/%s", name) >= (int)sizeof(path))
        throw std::runtime_error("Network namespace name is too long");

    UniqueFileDescriptor fd;
    if (!fd.OpenReadOnly(path))
        throw FormatErrno("Failed to open %s", path);

    return fd;
}

void
ReassociateNetworkNamespace(const char *name)
{
    assert(name != nullptr);

    if (setns(OpenNetworkNS(name).Get(), CLONE_NEWNET) < 0)
        throw FormatErrno("Failed to reassociate with network namespace '%s'",
                          name);
}
