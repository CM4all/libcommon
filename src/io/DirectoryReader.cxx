// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "DirectoryReader.hxx"
#include "UniqueFileDescriptor.hxx"
#include "system/Error.hxx"

#include <utility>

DirectoryReader::DirectoryReader(const char *path)
	:dir(opendir(path))
{
	if (dir == nullptr)
		throw FormatErrno("Failed to open directory %s", path);
}

static DIR *
OpenDir(UniqueFileDescriptor &&fd)
{
	auto dir = fdopendir(fd.Get());
	if (dir == nullptr)
		throw MakeErrno("Failed to reopen directory");

	fd.Steal();
	return dir;
}

DirectoryReader::DirectoryReader(UniqueFileDescriptor &&fd)
	:dir(OpenDir(std::move(fd))) {}

FileDescriptor
DirectoryReader::GetFileDescriptor() const noexcept
{
	return FileDescriptor(dirfd(dir));
}
