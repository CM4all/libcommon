// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupWatch.hxx"
#include "CgroupState.hxx"
#include "event/Loop.hxx"
#include "io/Open.hxx"
#include "io/ScopeChdir.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"

#include <stdlib.h>

static UniqueFileDescriptor
OpenMemoryUsage(const CgroupState &state)
{
	return OpenReadOnly(state.group_fd, "memory.current");
}

static uint64_t
ReadUint64(FileDescriptor fd)
{
	char buffer[64];
	ssize_t nbytes = fd.ReadAt(0, buffer, sizeof(buffer));
	if (nbytes < 0)
		throw MakeErrno("Failed to read cgroup file");

	if ((size_t)nbytes >= sizeof(buffer))
		throw std::runtime_error("Cgroup file is too large");

	char *endptr;
	uint64_t value = strtoull(buffer, &endptr, 10);
	if (endptr == buffer)
		throw std::runtime_error("Failed to parse cgroup file");

	return value;
}

CgroupMemoryWatch::CgroupMemoryWatch(EventLoop &event_loop,
				     const CgroupState &state,
				     BoundMethod<void(uint64_t value) noexcept> _callback)
	:fd(OpenMemoryUsage(state)),
	 inotify(event_loop, *this),
	 callback(_callback)
{
	{
		const ScopeChdir scope_chdir{state.group_fd};
		inotify.AddModifyWatch("memory.events.local");
	}
}

void
CgroupMemoryWatch::OnInotify(int, unsigned, const char *)
{
	const auto now = GetEventLoop().SteadyNow();
	if (now < next_time)
		// throttle
		return;

	next_time = now + std::chrono::seconds{30};

	uint64_t value = UINT64_MAX;

	try {
		value = ReadUint64(fd);
	} catch (...) {
		PrintException(std::current_exception());
	}

	callback(value);
}

void
CgroupMemoryWatch::OnInotifyError(std::exception_ptr error) noexcept
{
	PrintException(error);
}
