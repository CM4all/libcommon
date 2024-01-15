// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "StateDirectories.hxx"
#include "UniqueFileDescriptor.hxx"
#include "util/NumberParser.hxx"
#include "util/SpanCast.hxx"
#include "util/StringStrip.hxx"

#include <fmt/core.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>

StateDirectories::StateDirectories() noexcept
{
	AddDirectory("/lib/cm4all/state");
	AddDirectory("/var/lib/cm4all/state");
	AddDirectory("/etc/cm4all/state");
	AddDirectory("/run/cm4all/state");
}

StateDirectories::~StateDirectories() noexcept = default;

inline void
StateDirectories::AddDirectory(const char *path) noexcept
{
	UniqueFileDescriptor fd;
	if (fd.Open(path, O_PATH|O_DIRECTORY))
		directories.emplace_front(std::move(fd));
}

UniqueFileDescriptor
StateDirectories::OpenFile(const char *relative_path) const noexcept
{
	UniqueFileDescriptor fd;

	for (const auto &i : directories) {
		if (fd.OpenReadOnly(i, relative_path))
			break;

		const int e = errno;
		if (e != ENOENT)
			fmt::print(stderr, "Failed to open '{}': {}\n",
				   relative_path, strerror(e));
	}

	return fd;
}

std::span<const std::byte>
StateDirectories::GetBinary(const char *relative_path,
			    std::span<std::byte> buffer) const noexcept
{
	const auto fd = OpenFile(relative_path);
	if (!fd.IsDefined())
		return {};


	const auto nbytes = fd.Read(buffer);
	if (nbytes < 0) {
		fmt::print(stderr, "Failed to read '{}': {}\n",
			   relative_path, strerror(errno));
		return {};
	}

	return buffer.first(nbytes);
}

int
StateDirectories::GetInt(const char *relative_path, int default_value) const noexcept
{
	std::byte buffer[64];
	const auto r = GetBinary(relative_path, buffer);
	const auto value = ParseInteger<int>(Strip(ToStringView(r)));
	return value
		? *value
		: default_value;
}

unsigned
StateDirectories::GetUnsigned(const char *relative_path, unsigned default_value) const noexcept
{
	std::byte buffer[64];
	const auto r = GetBinary(relative_path, buffer);
	const auto value = ParseInteger<unsigned>(Strip(ToStringView(r)));
	return value
		? *value
		: default_value;
}

bool
StateDirectories::GetBool(const char *relative_path, bool default_value) const noexcept
{
	std::byte buffer[32];
	const auto r = GetBinary(relative_path, buffer);
	const auto value = ParseInteger<unsigned>(Strip(ToStringView(r)));
	return value && (*value == 0 || *value == 1)
		? static_cast<bool>(*value)
		: default_value;
}
