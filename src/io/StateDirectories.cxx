// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "StateDirectories.hxx"
#include "UniqueFileDescriptor.hxx"
#include "system/linux/openat2.h"
#include "util/IterableSplitString.hxx"
#include "util/NumberParser.hxx"
#include "util/SpanCast.hxx"
#include "util/StringSplit.hxx"
#include "util/StringStrip.hxx"

#include <fmt/core.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>

using std::string_view_literals::operator""sv;

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

/**
 * Open the specified directory as an O_PATH descriptor, but don't
 * follow any symlinks while resolving the given path.
 */
static FileDescriptor
OpenReadOnlyNoFollow(FileDescriptor directory, const char *path)
{
	static constexpr struct open_how how{
		.flags = O_RDONLY|O_NOFOLLOW|O_NOCTTY|O_CLOEXEC|O_NONBLOCK,
		.resolve = RESOLVE_IN_ROOT|RESOLVE_NO_MAGICLINKS|RESOLVE_NO_SYMLINKS,
	};

	int fd = openat2(directory.Get(), path, &how, sizeof(how));
	return FileDescriptor{fd};
}

static void
AppendPathSegment(std::string &dest, std::string_view segment) noexcept
{
	if (!dest.empty())
		dest.push_back('/');
	dest.append(segment);
}

static std::string
ResolveSymlink(std::string_view symlink_path,
	       std::string_view target,
	       std::string_view relative_path)
{
	if (target.starts_with('/')) {
		target.remove_prefix(1);
		symlink_path = {};
	} else if (auto slash = symlink_path.rfind('/');
		   slash != symlink_path.npos) {
		symlink_path = symlink_path.substr(0, slash);
	}

	std::string result{symlink_path};

	for (const std::string_view segment : IterableSplitString(target, '/')) {
		if (segment.empty() || segment == "."sv)
			continue;

		if (segment == ".."sv) {
			auto i = result.rfind('/');
			if (i == result.npos) {
				if (result.empty())
					return {};

				result.clear();
			} else {
				result.erase(i);
			}
		} else {
			AppendPathSegment(result, segment);
		}
	}

	if (!relative_path.empty())
		AppendPathSegment(result, relative_path);

	return result;
}

inline UniqueFileDescriptor
StateDirectories::OpenFileFollow(FileDescriptor directory_fd,
				 const char *_relative_path,
				 unsigned follow_limit) const noexcept
{
	std::string relative_path{_relative_path};

	std::string::size_type slash_pos = 0;

	while (true) {
		slash_pos = relative_path.find('/', slash_pos + 1);
		if (slash_pos != relative_path.npos)
			/* truncate the path at the current slash */
			relative_path[slash_pos] = '\0';

		char buffer[4096];
		std::string_view target;

		if (auto length = readlinkat(directory_fd.Get(),
					     relative_path.c_str(),
					     buffer, sizeof(buffer));
		    length < 0) {
			const int e = errno;
			if (e == EINVAL && slash_pos != relative_path.npos) {
				/* not a symlink - try the next path
				   segment */

				/* restore the slash */
				relative_path[slash_pos] = '/';

				/* try again */
				continue;
			}

			/* an unexpected error - bail out */
			fmt::print(stderr, "Failed to read symlink {:?}: {}\n",
				   relative_path, strerror(e));
			return {};
		} else if (static_cast<std::size_t>(length) == sizeof(buffer))
			/* symlink target is too long - bail out */
			return {};
		else
			target = {buffer, static_cast<std::size_t>(length)};

		const auto [symlink_path, rest] = slash_pos != relative_path.npos
			? PartitionWithout(std::string_view{relative_path}, slash_pos)
			: std::pair{std::string_view{relative_path}, std::string_view{}};
		const auto new_path = ResolveSymlink(symlink_path, target, rest);
		return OpenFileAutoFollow(new_path.c_str(), follow_limit);
	}
}

inline UniqueFileDescriptor
StateDirectories::OpenFileAutoFollow(const char *relative_path,
				     const unsigned follow_limit) const noexcept
{
	for (const auto &i : directories) {
		if (const auto fd = OpenReadOnlyNoFollow(i, relative_path);
		    fd.IsDefined())
			return UniqueFileDescriptor{fd};

		const int e = errno;

		if (e == ELOOP && follow_limit > 0) {
			/* there's a symlink somewhere in the chain -
			   find it and follow it manually, so symlinks
			   can point to arbitrary state directories */
			if (auto fd = OpenFileFollow(i, relative_path, follow_limit - 1);
			    fd.IsDefined())
				return fd;
		}

		if (e != ENOENT)
			fmt::print(stderr, "Failed to open {:?}: {}\n",
				   relative_path, strerror(e));
	}

	return {};
}

UniqueFileDescriptor
StateDirectories::OpenFile(const char *relative_path) const noexcept
{
	return OpenFileAutoFollow(relative_path, 8);
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
		fmt::print(stderr, "Failed to read {:?}: {}\n",
			   relative_path, strerror(errno));
		return {};
	}

	return buffer.first(nbytes);
}

signed
StateDirectories::GetSigned(const char *relative_path, int default_value) const noexcept
{
	std::byte buffer[64];
	const auto r = GetBinary(relative_path, buffer);
	const auto value = ParseInteger<signed>(Strip(ToStringView(r)));
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
