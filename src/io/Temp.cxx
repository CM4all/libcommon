// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
