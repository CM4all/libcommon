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
	if (!state.memory_v2)
		throw std::runtime_error("Cgroup2 controller 'memory' not found");

	const auto group = state.GetUnifiedGroupMount();
	if (!group.IsDefined())
		throw std::runtime_error("Cgroup controller 'memory' not found");

	return OpenReadOnly(group, "memory.current");
}

static uint64_t
ReadUint64(FileDescriptor fd)
{
	char buffer[64];
	ssize_t nbytes = pread(fd.Get(), buffer, sizeof(buffer), 0);
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
		const ScopeChdir scope_chdir{state.GetUnifiedGroupMount()};
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
