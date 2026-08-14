// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "WriteFile.hxx"
#include "UniqueFileDescriptor.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "util/SpanCast.hxx"

#ifdef __linux__
#include "FileAt.hxx"
#endif

#include <span>

#include <fcntl.h>

using std::string_view_literals::operator""sv;

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

#ifdef __linux__

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

void
WriteExistingFile(FileAt file, std::string_view value)
{
	switch (TryWriteExistingFile(file, value)) {
	case WriteFileResult::SUCCESS:
		break;

	case WriteFileResult::ERROR:
		throw FmtErrno("Failed to write to {:?}"sv, file.name);

	case WriteFileResult::SHORT:
		throw FmtRuntimeError("Short write to {:?}"sv, file.name);
	}
}

#endif // __linux__
