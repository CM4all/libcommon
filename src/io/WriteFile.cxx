// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "WriteFile.hxx"
#include "FileAt.hxx"
#include "UniqueFileDescriptor.hxx"
#include "util/SpanCast.hxx"

#include <span>

#include <fcntl.h>

static WriteFileResult
TryWrite(FileDescriptor fd, std::span<const std::byte> value) noexcept
{
	ssize_t nbytes = fd.Write(value);
	if (nbytes < 0)
		return WriteFileResult::ERROR;
	else if (std::span<const std::byte>::size_type(nbytes) == value.size())
		return WriteFileResult::SUCCESS;
	else
		return WriteFileResult::SHORT;
}

static WriteFileResult
TryWriteExistingFile(const char *path, std::span<const std::byte> value) noexcept
{
	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_WRONLY))
		return WriteFileResult::ERROR;

	return TryWrite(fd, value);
}

WriteFileResult
TryWriteExistingFile(const char *path, std::string_view value) noexcept
{
	return TryWriteExistingFile(path, AsBytes(value));
}

static WriteFileResult
TryWriteExistingFile(FileAt file,
		     std::span<const std::byte> value) noexcept
{
	UniqueFileDescriptor fd;
	if (!fd.Open(file, O_WRONLY))
		return WriteFileResult::ERROR;

	return TryWrite(fd, value);
}

WriteFileResult
TryWriteExistingFile(FileAt file,
		     std::string_view value) noexcept
{
	return TryWriteExistingFile(file, AsBytes(value));
}
