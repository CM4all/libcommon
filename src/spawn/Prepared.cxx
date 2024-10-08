// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Prepared.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/StringCompare.hxx"

#include <fmt/core.h>

#include <string.h>
#include <unistd.h>

PreparedChildProcess::PreparedChildProcess() noexcept
{
}

PreparedChildProcess::~PreparedChildProcess() noexcept = default;

void
PreparedChildProcess::InsertWrapper(std::span<const char *const> w) noexcept
{
	args.insert(args.begin(), w.begin(), w.end());
}

void
PreparedChildProcess::SetEnv(std::string_view name, std::string_view value) noexcept
{
	assert(!name.empty());

	PutEnv(fmt::format("{}={}", name, value));
}

const char *
PreparedChildProcess::GetEnv(std::string_view name) const noexcept
{
	for (const char *i : env) {
		if (i == nullptr)
			break;

		const char *s = StringAfterPrefix(i, name);
		if (s != nullptr && *s == '=')
			return s + 1;
	}

	return nullptr;
}

const char *
PreparedChildProcess::GetJailedHome() const noexcept
{
	const char *home = ns.mount.GetJailedHome();
	if (home != nullptr && chroot != nullptr) {
		const char *suffix = StringAfterPrefix(home, chroot);
		if (suffix == nullptr)
			return nullptr;

		switch (*suffix) {
		case '\0':
			home = "/";
			break;

		case '/':
			home = suffix;
			break;

		default:
			return nullptr;
		}
	}

	return home;
}

const char *
PreparedChildProcess::Finish() noexcept
{
	assert(!args.empty());

	const char *path = exec_path;

	if (path == nullptr) {
		path = args.front();
		const char *slash = strrchr(path, '/');
		if (slash != nullptr && slash[1] != 0)
			args.front() = slash + 1;
	}

	args.push_back(nullptr);

	if (GetEnv("PATH") == nullptr)
		/* if no PATH was specified, use a sensible and secure
		   default */
		/* as a side effect, this overrides bash's insecure
		   default PATH which includes "." */
		env.push_back("PATH=/usr/local/bin:/usr/bin:/bin");

	env.push_back(nullptr);

	return path;
}
