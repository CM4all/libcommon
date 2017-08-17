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

#include "RefenceOptions.hxx"
#include "AllocatorPtr.hxx"
#include "system/Error.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/djbhash.h"

#include <algorithm>

#include <stdio.h>
#include <fcntl.h>
#include <string.h>

RefenceOptions::RefenceOptions(AllocatorPtr alloc, const RefenceOptions &src)
    :data(alloc.Dup(src.data))
{
}

inline unsigned
RefenceOptions::GetHash() const
{
    return djb_hash(data.data, data.size);
}

char *
RefenceOptions::MakeId(char *p) const
{
    if (!IsEmpty()) {
        *p++ = ';';
        *p++ = 'r';
        *p++ = 'f';
        p += sprintf(p, "%08x", GetHash());
    }

    return p;
}

inline void
RefenceOptions::Apply(FileDescriptor fd) const
{
    // TODO: set name, script

    auto p = data.begin();
    const auto end = data.end();

    while (true) {
        const auto n = std::find(p, end, '\0');
        ssize_t nbytes = fd.Write(p, n - p);
        if (nbytes < 0)
            throw MakeErrno("Failed to write to Refence");

        if (n == end)
            break;

        p = n + 1;
    }
}

void
RefenceOptions::Apply() const
{
    if (IsEmpty())
        return;

    constexpr auto path = "/proc/cm4all/refence/self";
    UniqueFileDescriptor fd;
    if (!fd.Open(path, O_WRONLY))
        throw MakeErrno("Failed to open Refence");

    Apply(fd.ToFileDescriptor());
}
