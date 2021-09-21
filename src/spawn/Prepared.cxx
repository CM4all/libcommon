/*
 * Copyright 2007-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
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

#include "Prepared.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/ConstBuffer.hxx"
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
	if (control_fd.Get() >= 3)
		control_fd.Close();
}

void
PreparedChildProcess::InsertWrapper(ConstBuffer<const char *> w) noexcept
{
	args.insert(args.begin(), w.begin(), w.end());
}

void
PreparedChildProcess::SetEnv(const char *name, const char *value) noexcept
{
	assert(name != nullptr);
	assert(value != nullptr);

	strings.emplace_front(name);
	auto &s = strings.front();
	s.push_back('=');
	s.append(value);
	PutEnv(s.c_str());
}

const char *
PreparedChildProcess::GetEnv(StringView name) const noexcept
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
PreparedChildProcess::SetControl(int fd) noexcept
{
	assert(fd != control_fd.Get());

	if (control_fd.Get() >= 3)
		control_fd.Close();
	control_fd = FileDescriptor{fd};
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
PreparedChildProcess::SetControl(UniqueFileDescriptor fd) noexcept
{
	SetControl(fd.Steal());
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
	SetControl(fd.Steal());
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
