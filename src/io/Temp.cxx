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

#include "Temp.hxx"
#include "FileDescriptor.hxx"
#include "system/Error.hxx"
#include "system/Urandom.hxx"
#include "util/HexFormat.hxx"

#include <stdio.h>
#include <sys/stat.h>

static StringBuffer<16>
RandomFilename() noexcept
{
	std::array<std::byte, 7> r;
	UrandomFill(&r, sizeof(r));

	StringBuffer<16> name;
	*HexFormat(name.data(), r) = 0;
	return name;
}

StringBuffer<16>
MakeTempDirectory(FileDescriptor parent_fd, mode_t mode)
{
	while (true) {
		auto name = RandomFilename();
		if (mkdirat(parent_fd.Get(), name.c_str(), mode) == 0)
			return name;

		switch (const int e = errno) {
		case EEXIST:
			/* try again with new name */
			continue;

		default:
			throw MakeErrno(e, "Failed to create directory");
		}
	}
}

StringBuffer<16>
MoveToTemp(FileDescriptor old_parent_fd, const char *old_name,
	   FileDescriptor new_parent_fd)
{
	while (true) {
		auto name = RandomFilename();

		if (renameat2(old_parent_fd.Get(), old_name,
			      new_parent_fd.Get(), name,
			      RENAME_NOREPLACE) == 0)
			return name;

		int e = errno;

		if (e == EINVAL) {
			/* RENAME_NOREPLACE not supported by the
			   filesystem */
			if (renameat(old_parent_fd.Get(), old_name,
				     new_parent_fd.Get(), name) == 0)
				return name;

			e = errno;
		}

		switch (e) {
		case EEXIST:
		case ENOTEMPTY:
			/* try again with new name */
			continue;

		default:
			throw MakeErrno(e, "Failed to create directory");
		}
	}
}
