// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Manager.hxx"
#include "util/PrintException.hxx"

namespace Uring {

Manager::Manager(EventLoop &event_loop,
		 unsigned entries, unsigned flags)
	:Queue(entries, flags),
	 event(event_loop, BIND_THIS_METHOD(OnReady),
	       GetFileDescriptor()),
	 defer_submit_event(event_loop,
			    BIND_THIS_METHOD(DeferredSubmit))
{
	event.ScheduleRead();
}

void
Manager::OnReady(unsigned) noexcept
{
	try {
		DispatchCompletions();
	} catch (...) {
		PrintException(std::current_exception());
	}

	CheckVolatileEvent();
}

void
Manager::DeferredSubmit() noexcept
{
	try {
		Queue::Submit();
	} catch (...) {
		PrintException(std::current_exception());
	}
}

} // namespace Uring
