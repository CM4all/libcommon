// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "FileLineParser.hxx"

namespace fs = std::filesystem;

static fs::path
ApplyPath(const fs::path &base, fs::path &&p)
{
	if (p.is_absolute())
		/* is already absolute */
		return std::move(p);

	return base.parent_path() / p;
}

fs::path
FileLineParser::ExpectPath()
{
	const char *value = NextUnescape();
	if (value == nullptr)
		throw Error("Quoted path expected");

	return ApplyPath(base_path, value);
}

fs::path
FileLineParser::ExpectPathAndEnd()
{
	auto value = ExpectPath();
	ExpectEnd();
	return value;
}
