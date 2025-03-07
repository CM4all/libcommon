// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CgroupWatch.hxx"
#include "CgroupState.hxx"
#include "event/Loop.hxx"
#include "io/Open.hxx"
#include "io/SmallTextFile.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "io/linux/ProcPath.hxx"
#include "system/Error.hxx"
#include "util/NumberParser.hxx"
#include "util/PrintException.hxx"
#include "util/SpanCast.hxx"
#include "util/StringStrip.hxx"

#include <string_view>

#include <fcntl.h> // for O_WRONLY

using std::string_view_literals::operator""sv;

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
	 pressure(event_loop, BIND_THIS_METHOD(OnPressure)),
	 callback(_callback)
{
	inotify.AddModifyWatch(ProcFdPath(OpenPath(group_fd, "memory.events")));

	if (UniqueFileDescriptor pressure_fd;
	    pressure_fd.Open(group_fd, "memory.pressure", O_WRONLY)) {
		/* subscribe to a notification every 2 seconds if the
		   "some" memory pressure goes above 10% */
		constexpr auto w = "some 200000 2000000"sv;
		if (pressure_fd.Write(AsBytes(w)) > 0) {
			pressure.Open(pressure_fd.Release());
			pressure.Schedule(pressure.EXCEPTIONAL);
		}
	}
}

CgroupMemoryWatch::~CgroupMemoryWatch() noexcept
{
	pressure.Close();
}

uint_least64_t
CgroupMemoryWatch::GetMemoryUsage() const
{
	return ReadUint64(fd);
}

void
CgroupMemoryWatch::OnPressure(unsigned events) noexcept
{
	if (events & pressure.ERROR) {
		pressure.Close();
		return;
	}

	uint_least64_t value = UINT64_MAX;

	try {
		value = GetMemoryUsage();
	} catch (...) {
		PrintException(std::current_exception());
	}

	callback(value);
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
