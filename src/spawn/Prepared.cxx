/*
 * Copyright 2007-2017 Content Management AG
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

#include <string.h>
#include <unistd.h>

PreparedChildProcess::PreparedChildProcess()
{
}

PreparedChildProcess::~PreparedChildProcess()
{
	/* note: the lower boundary here is 3 because we should never
	   close 0, 1 or 2; these are the standard file descriptors
	   and the caller still needs them */

	if (stdin_fd >= 3)
		close(stdin_fd);
	if (stdout_fd >= 3 && stdout_fd != stdin_fd)
		close(stdout_fd);
	if (stderr_fd >= 3 &&
	    stderr_fd != stdout_fd && stderr_fd != stdin_fd)
		close(stderr_fd);
	if (control_fd >= 3)
		close(control_fd);
}

void
PreparedChildProcess::InsertWrapper(ConstBuffer<const char *> w)
{
	args.insert(args.begin(), w.begin(), w.end());
}

void
PreparedChildProcess::SetEnv(const char *name, const char *value)
{
	assert(name != nullptr);
	assert(value != nullptr);

	strings.emplace_front(name);
	auto &s = strings.front();
	s.push_back('=');
	s.append(value);
	PutEnv(s.c_str());
}

void
PreparedChildProcess::SetStdin(int fd)
{
	assert(fd != stdin_fd);

	if (stdin_fd >= 3)
		close(stdin_fd);
	stdin_fd = fd;
}

void
PreparedChildProcess::SetStdout(int fd)
{
	assert(fd != stdout_fd);

	if (stdout_fd >= 3)
		close(stdout_fd);
	stdout_fd = fd;
}

void
PreparedChildProcess::SetStderr(int fd)
{
	assert(fd != stderr_fd);

	if (stderr_fd >= 3)
		close(stderr_fd);
	stderr_fd = fd;
}

void
PreparedChildProcess::SetControl(int fd)
{
	assert(fd != control_fd);

	if (control_fd >= 3)
		close(control_fd);
	control_fd = fd;
}

void
PreparedChildProcess::SetStdin(UniqueFileDescriptor fd)
{
	SetStdin(fd.Steal());
}

void
PreparedChildProcess::SetStdout(UniqueFileDescriptor fd)
{
	SetStdout(fd.Steal());
}

void
PreparedChildProcess::SetStderr(UniqueFileDescriptor fd)
{
	SetStderr(fd.Steal());
}

void
PreparedChildProcess::SetControl(UniqueFileDescriptor fd)
{
	SetControl(fd.Steal());
}

void
PreparedChildProcess::SetStdin(UniqueSocketDescriptor fd)
{
	SetStdin(fd.Steal());
}

void
PreparedChildProcess::SetStdout(UniqueSocketDescriptor fd)
{
	SetStdout(fd.Steal());
}

void
PreparedChildProcess::SetStderr(UniqueSocketDescriptor fd)
{
	SetStderr(fd.Steal());
}

void
PreparedChildProcess::SetControl(UniqueSocketDescriptor fd)
{
	SetControl(fd.Steal());
}

const char *
PreparedChildProcess::Finish()
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
	env.push_back(nullptr);

	return path;
}
