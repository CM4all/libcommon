/*
 * Copyright (C) 2011-2018 Max Kellermann <max.kellermann@gmail.com>
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

#include "FileWriter.hxx"
#include "system/Error.hxx"
#include "util/RuntimeError.hxx"

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

static std::string
GetDirectory(const char *path)
{
	const char *slash = strrchr(path, '/');
	if (slash == nullptr)
		return ".";

	if (slash == path)
		return "/";

	return std::string(path, slash);
}

static std::pair<std::string, UniqueFileDescriptor>
MakeTempFileInDirectory(const std::string &directory)
{
	unsigned r = rand();

	while (true) {
		auto path = directory + "/tmp." + std::to_string(r);
		UniqueFileDescriptor fd;
		if (fd.Open(path.c_str(), O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,
			    0666))
			return std::make_pair(std::move(path), std::move(fd));

		if (errno != EEXIST)
			throw FormatErrno("Failed to create %s", path.c_str());

		++r;
	}
}

FileWriter::FileWriter(const char *_path)
	:path(_path)
{
	const auto directory = GetDirectory(_path);

	if (fd.Open(directory.c_str(), O_TMPFILE|O_WRONLY, 0666))
		return;

	auto tmp = MakeTempFileInDirectory(directory);
	tmp_path = std::move(tmp.first);
	fd = std::move(tmp.second);
}

void
FileWriter::Allocate(off_t size)
{
	fallocate(fd.Get(), FALLOC_FL_KEEP_SIZE, 0, size);
}

void
FileWriter::Write(const void *data, size_t size)
{
	ssize_t nbytes = fd.Write(data, size);
	if (nbytes < 0)
		throw FormatRuntimeError("Failed to write to %s", path.c_str());

	if (size_t(nbytes) < size)
		throw FormatRuntimeError("Short write to %s", path.c_str());
}

void
FileWriter::Commit()
{
	assert(fd.IsDefined());

	if (tmp_path.empty()) {
		unlink(path.c_str());

		/* hard-link the temporary file to the final path */
		char fd_path[64];
		snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d",
			 fd.Get());
		if (linkat(AT_FDCWD, fd_path, AT_FDCWD, path.c_str(),
			   AT_SYMLINK_FOLLOW) < 0)
			throw FormatErrno("Failed to commit %s",
					  path.c_str());
	}

	if (!fd.Close())
		throw FormatErrno("Failed to commit %s", path.c_str());

	if (!tmp_path.empty() && rename(tmp_path.c_str(), path.c_str()) < 0)
		throw FormatErrno("Failed to rename %s to %s",
				  tmp_path.c_str(), path.c_str());
}

void
FileWriter::Cancel()
{
	assert(fd.IsDefined());

	fd.Close();

	if (!tmp_path.empty())
		unlink(tmp_path.c_str());
}
