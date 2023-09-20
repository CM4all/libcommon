// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "FileWriter.hxx"
#include "io/linux/ProcPath.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "lib/fmt/ToBuffer.hxx"

#include <cassert>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

static std::string
GetDirectory(const char *path) noexcept
{
	const char *slash = strrchr(path, '/');
	if (slash == nullptr)
		return ".";

	if (slash == path)
		return "/";

	return std::string(path, slash);
}

static std::pair<std::string, UniqueFileDescriptor>
MakeTempFileInDirectory(const std::string &directory, mode_t mode)
{
	unsigned r = rand();

	while (true) {
		auto path = directory + "/tmp." + std::to_string(r);
		UniqueFileDescriptor fd;
		if (fd.Open(path.c_str(), O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,
			    mode))
			return {std::move(path), std::move(fd)};

		if (errno != EEXIST)
			throw FmtErrno("Failed to create {}", path);

		++r;
	}
}

static std::pair<std::string, UniqueFileDescriptor>
MakeTempFileInDirectory(FileDescriptor directory_fd, mode_t mode)
{
	unsigned r = rand();

	while (true) {
		auto path = "tmp." + std::to_string(r);
		UniqueFileDescriptor fd;
		if (fd.Open(directory_fd, path.c_str(),
			    O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,
			    mode))
			return {std::move(path), std::move(fd)};

		if (errno != EEXIST)
			throw FmtErrno("Failed to create {}", path);

		++r;
	}
}

FileWriter::FileWriter(FileDescriptor _directory_fd, const char *_path,
		       mode_t mode)
	:path(_path), directory_fd(_directory_fd)
{
	if (directory_fd != FileDescriptor(AT_FDCWD)) {
		if (fd.Open(directory_fd, ".", O_TMPFILE|O_WRONLY, mode))
			return;

		auto tmp = MakeTempFileInDirectory(directory_fd, mode);
		tmp_path = std::move(tmp.first);
		fd = std::move(tmp.second);
	} else {
		const auto directory = GetDirectory(_path);

		if (fd.Open(directory.c_str(), O_TMPFILE|O_WRONLY, mode))
			return;

		auto tmp = MakeTempFileInDirectory(directory, mode);
		tmp_path = std::move(tmp.first);
		fd = std::move(tmp.second);
	}
}

FileWriter::FileWriter(const char *_path, mode_t mode)
	:FileWriter(FileDescriptor(AT_FDCWD), _path, mode) {}

void
FileWriter::Allocate(off_t size) noexcept
{
	fallocate(fd.Get(), FALLOC_FL_KEEP_SIZE, 0, size);
}

void
FileWriter::Write(const void *data, size_t size)
{
	ssize_t nbytes = fd.Write(data, size);
	if (nbytes < 0)
		throw FmtRuntimeError("Failed to write to {}", path);

	if (size_t(nbytes) < size)
		throw FmtRuntimeError("Short write to {}", path);
}

void
FileWriter::Commit()
{
	assert(fd.IsDefined());

	if (tmp_path.empty()) {
		unlinkat(directory_fd.Get(), path.c_str(), 0);

		/* hard-link the temporary file to the final path */
		if (linkat(AT_FDCWD, ProcFdPath(fd),
			   directory_fd.Get(), path.c_str(),
			   AT_SYMLINK_FOLLOW) < 0)
			throw FmtErrno("Failed to commit {}", path);
	}

	if (!fd.Close())
		throw FmtErrno("Failed to commit {}", path);

	if (!tmp_path.empty() &&
	    renameat(directory_fd.Get(), tmp_path.c_str(),
		     directory_fd.Get(), path.c_str()) < 0)
		throw FmtErrno("Failed to rename {} to {}",
			       tmp_path, path);
}

void
FileWriter::Cancel() noexcept
{
	assert(fd.IsDefined());

	fd.Close();

	if (!tmp_path.empty())
		unlinkat(directory_fd.Get(), tmp_path.c_str(), 0);
}
