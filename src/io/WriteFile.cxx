/*
 * Copyright 2015-2018 Max Kellermann <max.kellermann@gmail.com>
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

#include "WriteFile.hxx"
#include "UniqueFileDescriptor.hxx"
#include "util/ConstBuffer.hxx"
#include "util/StringView.hxx"

#include <errno.h>
#include <fcntl.h>

static WriteFileResult
TryWrite(FileDescriptor fd, ConstBuffer<void> value) noexcept
{
	ssize_t nbytes = fd.Write(value.data, value.size);
	if (nbytes < 0)
		return WriteFileResult::ERROR;
	else if (size_t(nbytes) == value.size)
		return WriteFileResult::SUCCESS;
	else
		return WriteFileResult::SHORT;
}

static WriteFileResult
TryWriteExistingFile(const char *path, ConstBuffer<void> value) noexcept
{
	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_WRONLY))
		return WriteFileResult::ERROR;

	return TryWrite(fd, value);
}

WriteFileResult
TryWriteExistingFile(const char *path, const char *value) noexcept
{
	return TryWriteExistingFile(path, StringView(value).ToVoid());
}

static WriteFileResult
TryWriteExistingFile(FileDescriptor directory, const char *path,
		     ConstBuffer<void> value) noexcept
{
	UniqueFileDescriptor fd;
	if (!fd.Open(directory, path, O_WRONLY))
		return WriteFileResult::ERROR;

	return TryWrite(fd, value);
}

WriteFileResult
TryWriteExistingFile(FileDescriptor directory, const char *path,
		     const char *value) noexcept
{
	return TryWriteExistingFile(directory, path,
				    StringView(value).ToVoid());
}
