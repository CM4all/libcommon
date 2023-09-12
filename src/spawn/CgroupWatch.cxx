// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupWatch.hxx"
#include "CgroupState.hxx"
#include "event/Loop.hxx"
#include "io/Open.hxx"
#include "io/ScopeChdir.hxx"
#include "io/SmallTextFile.hxx"
#include "system/Error.hxx"
#include "util/NumberParser.hxx"
#include "util/PrintException.hxx"
#include "util/StringStrip.hxx"

static UniqueFileDescriptor
OpenMemoryUsage(const CgroupState &state)
{
	return OpenReadOnly(state.group_fd, "memory.current");
}

static uint64_t
ReadUint64(FileDescriptor fd)
{
	return WithSmallTextFile<64>(fd, [](std::string_view contents){
		const auto value = ParseInteger<uint64_t>(StripRight(contents));
		if (!value)
			throw std::runtime_error("Failed to parse cgroup file");

		return *value;
	});
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

	next_time = now + std::chrono::seconds{10};

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
