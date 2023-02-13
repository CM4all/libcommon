// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "StringFile.hxx"
#include "system/Error.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringStrip.hxx"

std::string
LoadStringFile(const char *path)
{
	auto fd = OpenReadOnly(path);

	char buffer[1024];
	ssize_t nbytes = fd.Read(buffer, sizeof(buffer));
	if (nbytes < 0)
		throw FormatErrno("Failed to read %s", path);

	size_t length = nbytes;
	if (length >= sizeof(buffer))
		throw FormatRuntimeError("File is too large: %s", path);

	const char *begin = buffer;
	const char *end = buffer + length;

	begin = StripLeft(begin, end);
	end = StripRight(begin, end);

	return std::string{begin, end};
}
