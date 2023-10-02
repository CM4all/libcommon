// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CopyRegularFile.hxx"
#include "FileDescriptor.hxx"
#include "lib/fmt/SystemError.hxx"

#include <array>
#include <cstddef>

#include <fcntl.h> // for posix_fadvise(), fallocate()

/**
 * Throws on error.
 *
 * @return true on success (all data has been copied), false if
 * copy_file_range() is not supported (no data has been copied)
 */
static bool
CopyFileRange(FileDescriptor src, FileDescriptor dst, off_t size)
{
	/* check if copy_file_range() works */
	auto nbytes = copy_file_range(src.Get(), nullptr, dst.Get(), nullptr,
				      size, 0);
	if (nbytes <= 0)
		/* nah */
		return false;

	/* hooray, copy_file_range() works */
	size -= nbytes;

	while (size > 0) {
		nbytes = copy_file_range(src.Get(), nullptr,
					 dst.Get(), nullptr,
					 size, 0);
		if (nbytes <= 0) [[unlikely]] {
			if (nbytes == 0)
				throw std::runtime_error{"Unexpected end of file"};

			throw MakeErrno("Failed to copy file data");
		}

		size -= nbytes;
	}

	return true;
}

void
CopyRegularFile(FileDescriptor src, FileDescriptor dst, off_t size)
{
	if (size <= 0)
		return;

	if (CopyFileRange(src, dst, size))
		return;

	posix_fadvise(src.Get(), 0, size, POSIX_FADV_SEQUENTIAL);

	fallocate(dst.Get(), FALLOC_FL_KEEP_SIZE, 0, size);

	while (size > 0) {
		std::array<std::byte, 65536> buffer;
		const auto nbytes1 = src.Read(buffer);
		if (nbytes1 <= 0) [[unlikely]] {
			if (nbytes1 == 0)
				throw std::runtime_error{"Unexpected end of file"};

			throw MakeErrno("Failed to read file");
		}

		ssize_t nbytes2 = dst.Write(std::span<const std::byte>{buffer}.first(nbytes1));
		if (nbytes2 < nbytes1) [[unlikely]] {
			if (nbytes2 < 0)
				throw MakeErrno("Failed to write file");

			throw std::runtime_error{"Short write"};
		}

		size -= nbytes2;
	}
}
