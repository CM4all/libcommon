// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Prepared.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/StringCompare.hxx"

#include <string.h>
#include <unistd.h>

PreparedChildProcess::PreparedChildProcess() noexcept
{
}

PreparedChildProcess::~PreparedChildProcess() noexcept
{
	/* note: the lower boundary here is 3 because we should never
	   close 0, 1 or 2; these are the standard file descriptors
	   and the caller still needs them */

	if (stderr_fd.Get() >= 3 &&
	    stderr_fd != stdout_fd && stderr_fd != stdin_fd)
		stderr_fd.Close();
	if (stdout_fd.Get() >= 3 && stdout_fd != stdin_fd)
		stdout_fd.Close();
	if (stdin_fd.Get() >= 3)
		stdin_fd.Close();
}

void
PreparedChildProcess::InsertWrapper(std::span<const char *const> w) noexcept
{
	args.insert(args.begin(), w.begin(), w.end());
}

void
PreparedChildProcess::SetEnv(std::string_view name, std::string_view value) noexcept
{
	assert(!name.empty());

	strings.emplace_front(name);
	auto &s = strings.front();
	s.push_back('=');
	s.append(value);
	PutEnv(s.c_str());
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

void
PreparedChildProcess::SetStdin(int fd) noexcept
{
	assert(fd != stdin_fd.Get());

	if (stdin_fd.Get() >= 3)
		stdin_fd.Close();
	stdin_fd = FileDescriptor{fd};
}

void
PreparedChildProcess::SetStdout(int fd) noexcept
{
	assert(fd != stdout_fd.Get());

	if (stdout_fd.Get() >= 3)
		stdout_fd.Close();
	stdout_fd = FileDescriptor{fd};
}

void
PreparedChildProcess::SetStderr(int fd) noexcept
{
	assert(fd != stderr_fd.Get());

	if (stderr_fd.Get() >= 3)
		stderr_fd.Close();
	stderr_fd = FileDescriptor{fd};
}

void
PreparedChildProcess::SetStdin(UniqueFileDescriptor fd) noexcept
{
	SetStdin(fd.Steal());
}

void
PreparedChildProcess::SetStdout(UniqueFileDescriptor fd) noexcept
{
	SetStdout(fd.Steal());
}

void
PreparedChildProcess::SetStderr(UniqueFileDescriptor fd) noexcept
{
	SetStderr(fd.Steal());
}

void
PreparedChildProcess::SetStdin(UniqueSocketDescriptor fd) noexcept
{
	SetStdin(fd.Steal());
}

void
PreparedChildProcess::SetStdout(UniqueSocketDescriptor fd) noexcept
{
	SetStdout(fd.Steal());
}

void
PreparedChildProcess::SetStderr(UniqueSocketDescriptor fd) noexcept
{
	SetStderr(fd.Steal());
}

void
PreparedChildProcess::SetControl(UniqueSocketDescriptor fd) noexcept
{
	SetControl(std::move(fd).MoveToFileDescriptor());
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
