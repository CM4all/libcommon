// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupWatch.hxx"
#include "CgroupState.hxx"
#include "event/Loop.hxx"
#include "io/Open.hxx"
#include "io/SmallTextFile.hxx"
#include "io/linux/ProcPath.hxx"
#include "system/Error.hxx"
#include "util/NumberParser.hxx"
#include "util/PrintException.hxx"
#include "util/StringStrip.hxx"

static UniqueFileDescriptor
OpenMemoryUsage(FileDescriptor group_fd)
{
	return OpenReadOnly(group_fd, "memory.current");
}

static uint_least64_t
ReadUint64(FileDescriptor fd)
{
	return WithSmallTextFile<64>(fd, [](std::string_view contents){
		const auto value = ParseInteger<uint_least64_t>(StripRight(contents));
		if (!value)
			throw std::runtime_error("Failed to parse cgroup file");

		return *value;
	});
}

CgroupMemoryWatch::CgroupMemoryWatch(EventLoop &event_loop,
				     FileDescriptor group_fd,
				     BoundMethod<void(uint_least64_t value) noexcept> _callback)
	:fd(OpenMemoryUsage(group_fd)),
	 inotify(event_loop, *this),
	 callback(_callback)
{
	inotify.AddModifyWatch(ProcFdPath(OpenPath(group_fd, "memory.events.local")));
}

uint_least64_t
CgroupMemoryWatch::GetMemoryUsage() const
{
	return ReadUint64(fd);
}

void
CgroupMemoryWatch::OnInotify(int, unsigned, const char *)
{
	uint_least64_t value = UINT64_MAX;

	try {
		value = GetMemoryUsage();
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
