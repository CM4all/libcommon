// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SignalEvent.hxx"
#include "system/Error.hxx"

#include <sys/signalfd.h>
#include <unistd.h>

SignalEvent::SignalEvent(EventLoop &loop, Callback _callback) noexcept
	:event(loop, BIND_THIS_METHOD(EventCallback)), callback(_callback)
{
	sigemptyset(&mask);
}

void
SignalEvent::Enable()
{
	assert(!IsDefined());

	int fd = signalfd(-1, &mask, SFD_NONBLOCK|SFD_CLOEXEC);
	if (fd < 0)
		throw MakeErrno("signalfd() failed");

	event.Open(FileDescriptor{fd});
	event.ScheduleRead();

	sigprocmask(SIG_BLOCK, &mask, nullptr);
}

void
SignalEvent::Disable() noexcept
{
	if (!IsDefined())
		return;

	sigprocmask(SIG_UNBLOCK, &mask, nullptr);

	event.Close();
}

void
SignalEvent::EventCallback(unsigned) noexcept
{
	struct signalfd_siginfo info;
	ssize_t nbytes = event.GetFileDescriptor().Read(std::as_writable_bytes(std::span{&info, 1}));
	if (nbytes <= 0) {
		// TODO: log error?
		Disable();
		return;
	}

	callback(info.ssi_signo);
}
