/*
 * Copyright 2022 CM4all GmbH
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

#include "RecursiveDelete.hxx"
#include "DirectoryReader.hxx"
#include "Open.hxx"
#include "UniqueFileDescriptor.hxx"
#include "system/Error.hxx"

#include <fcntl.h>
#include <unistd.h>

[[gnu::pure]]
static bool
IsSpecialFilename(const char *s) noexcept
{
    return s[0]=='.' && (s[1] == 0 || (s[1] == '.' && s[2] == 0));
}

static void
ClearDirectory(UniqueFileDescriptor &&fd)
{
	DirectoryReader r{std::move(fd)};

	while (const char *child = r.Read())
		if (!IsSpecialFilename(child))
			RecursiveDelete(r.GetFileDescriptor(), child);
}

static void
RecursiveDeleteDirectory(FileDescriptor parent, const char *filename)
{
	ClearDirectory(OpenDirectory(parent, filename, O_NOFOLLOW));

	if (unlinkat(parent.Get(), filename, AT_REMOVEDIR) == 0)
		return;

	switch (const int e = errno; e) {
	case ENOENT:
		/* does not exist, nothing to do */
		return;

	default:
		throw FormatErrno(e, "Failed to delete %s", filename);
	}
}

void
RecursiveDelete(FileDescriptor parent, const char *filename)
{
	if (unlinkat(parent.Get(), filename, 0) == 0)
		return;

	switch (const int e = errno; e) {
	case EISDIR:
		/* switch to directory mode */
		RecursiveDeleteDirectory(parent, filename);
		return;

	case ENOENT:
		/* does not exist, nothing to do */
		return;

	default:
		throw FormatErrno(e, "Failed to delete %s", filename);
	}
}
