// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "CgroupOptions.hxx"
#include "CgroupState.hxx"
#include "MakeId.hxx"
#include "AllocatorPtr.hxx"
#include "lib/fmt/SystemError.hxx"
#include "io/FileAt.hxx"
#include "io/MakeDirectory.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/WriteFile.hxx"
#include "util/CharUtil.hxx"
#include "util/StringAPI.hxx"
#include "util/StringSplit.hxx"
#include "util/StringVerify.hxx"
#include "util/StringListVerify.hxx"

#include <fmt/format.h>

#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/xattr.h>

using std::string_view_literals::operator""sv;

CgroupOptions::CgroupOptions(AllocatorPtr alloc,
			     const CgroupOptions &src) noexcept
	:name(alloc.CheckDup(src.name)),
	 xattr(alloc, src.xattr),
	 set(alloc, src.set)
{
}

static constexpr bool
IsValidCgroupNameChar(char ch) noexcept
{
	return IsLowerAlphaASCII(ch) || ch == '_';
}

static constexpr bool
IsValidCgroupName(std::string_view s) noexcept
{
	return CheckCharsNonEmpty(s, IsValidCgroupNameChar);
}

static constexpr bool
IsValidCgroupAttributeNameChar(char ch) noexcept
{
	return IsLowerAlphaASCII(ch) || ch == '_';
}

static constexpr bool
IsValidCgroupAttributeNameSegment(std::string_view s) noexcept
{
	return CheckCharsNonEmpty(s, IsValidCgroupAttributeNameChar);
}

static constexpr bool
IsValidCgroupAttributeName(std::string_view s) noexcept
{
	return IsNonEmptyListOf(s, '.', IsValidCgroupAttributeNameSegment);
}

bool
CgroupOptions::IsValidSetName(std::string_view name) noexcept
{
	const auto [controller, attribute] = Split(name, '.');
	if (!IsValidCgroupName(controller) ||
	    !IsValidCgroupAttributeName(attribute))
		return false;

	if (controller == "cgroup"sv)
		/* this is not a controller, this is a core cgroup
		   attribute */
		return false;

	return true;
}

bool
CgroupOptions::IsValidSetValue(std::string_view value) noexcept
{
	return !value.empty() && value.find('/') == value.npos;
}

void
CgroupOptions::SetXattr(AllocatorPtr alloc,
			std::string_view _name, std::string_view _value) noexcept
{
	xattr.Add(alloc, _name, _value);
}

void
CgroupOptions::Set(AllocatorPtr alloc,
		   std::string_view _name, std::string_view _value) noexcept
{
	set.Add(alloc, _name, _value);
}

static void
WriteCgroupFile(FileDescriptor group_fd,
		const char *filename, std::string_view value)
{
	/* emulate cgroup1 for old translation servers */
	if (StringIsEqual(filename, "memory.limit_in_bytes"))
		filename = "memory.max";

	WriteExistingFile({group_fd, filename}, value);
}

CgroupOptions::CreateResult
CgroupOptions::Create2(const CgroupState &state, const char *session) const
{
	if (name == nullptr)
		return {};

	if (!state.IsEnabled())
		throw std::runtime_error("Control groups are disabled");

	auto fd = MakeDirectory({state.group_fd, name},
				{.follow_symlinks = false});

	if (!xattr.empty()) {
		/* reopen the directory because fsetxattr() refuses to
		   work with an O_PATH file descriptor */
		auto d = OpenDirectory({fd, "."});

		for (const auto &i : xattr)
			if (fsetxattr(d.Get(), i.name,
				      i.value, strlen(i.value), 0) < 0)
				throw FmtErrno("Failed to set xattr {:?}",
					       i.name);
	}

	for (const auto &s : set)
		WriteCgroupFile(fd, s.name, s.value);

	if (session != nullptr) {
		auto session_fd = MakeDirectory({fd, session});
		return {std::move(fd), std::move(session_fd)};
	} else
		return {std::move(fd), {}};
}

char *
CgroupOptions::MakeId(char *p) const noexcept
{
	p = AppendOptionalValue(p, ";cg"sv, name);
	return p;
}
