// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "CgroupPidsWatch.hxx"
#include "io/Open.hxx"
#include "io/SmallTextFile.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/linux/ProcPath.hxx"
#include "util/NumberParser.hxx"
#include "util/PrintException.hxx"
#include "util/StringStrip.hxx"

#include <string_view>

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

CgroupPidsWatch::CgroupPidsWatch(EventLoop &event_loop,
				 FileDescriptor group_fd,
				 BoundMethod<void(uint_least64_t value) noexcept> _callback)
	:current_fd(OpenReadOnly(group_fd, "pids.current")),
	 inotify(event_loop, *this),
	 callback(_callback)
{
	inotify.AddModifyWatch(ProcFdPath(OpenPath(group_fd, "pids.events")));
}

CgroupPidsWatch::~CgroupPidsWatch() noexcept = default;

uint_least64_t
CgroupPidsWatch::GetPidsCurrent() const
{
	return ReadUint64(current_fd);
}

void
CgroupPidsWatch::OnInotify(int, unsigned, const char *)
{
	uint_least64_t value = UINT64_MAX;

	try {
		value = GetPidsCurrent();
	} catch (...) {
		PrintException(std::current_exception());
	}

	callback(value);
}

void
CgroupPidsWatch::OnInotifyError(std::exception_ptr error) noexcept
{
	PrintException(error);
}
