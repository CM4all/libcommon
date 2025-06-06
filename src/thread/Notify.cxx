// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Notify.hxx"
#include "system/LinuxFD.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "util/SpanCast.hxx"

Notify::Notify(EventLoop &event_loop, Callback _callback) noexcept
	:callback(_callback),
	 event(event_loop, BIND_THIS_METHOD(EventFdCallback),
	       CreateEventFD().Release())
{
	event.ScheduleRead();
}

Notify::~Notify() noexcept
{
	event.Close();
}

void
Notify::Signal() noexcept
{
	if (!pending.exchange(true)) {
		static constexpr uint64_t value = 1;
		(void)event.GetFileDescriptor()
			.Write(ReferenceAsBytes(value));
	}
}

inline void
Notify::EventFdCallback(unsigned) noexcept
{
	uint64_t value;
	(void)event.GetFileDescriptor().Read(std::as_writable_bytes(std::span{&value, 1}));

	if (pending.exchange(false))
		callback();
}
