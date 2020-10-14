/*
 * Copyright 2017-2020 CM4all GmbH
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

#include "SocketEvent.hxx"
#include "Loop.hxx"

#include <cassert>
#include <utility>

void
SocketEvent::Open(SocketDescriptor _fd) noexcept
{
	assert(_fd.IsDefined());
	assert(GetScheduledFlags() == 0);

	fd = _fd;
}

void
SocketEvent::Close() noexcept
{
	if (!fd.IsDefined())
		return;

	/* closing the socket automatically unregisters it from epoll,
	   so we can omit the epoll_ctl(EPOLL_CTL_DEL) call and save
	   one system call */
	fd.Close();
	Abandon();
}

void
SocketEvent::Schedule(unsigned flags) noexcept
{
	assert((flags & IMPLICIT_FLAGS) == 0);

	if (flags == GetScheduledFlags())
		return;

	assert(IsDefined());

	if (scheduled_flags == 0)
		loop.AddFD(fd.Get(), flags, *this);
	else if (flags == 0)
		loop.RemoveFD(fd.Get(), *this);
	else
		loop.ModifyFD(fd.Get(), flags, *this);

	scheduled_flags = flags;
}

void
SocketEvent::ScheduleImplicit() noexcept
{
	assert(IsDefined());
	assert(scheduled_flags == 0);

	scheduled_flags = IMPLICIT_FLAGS;
	loop.AddFD(fd.Get(), scheduled_flags, *this);
}

void
SocketEvent::Abandon() noexcept
{
	fd = SocketDescriptor::Undefined();
	if (std::exchange(scheduled_flags, 0) != 0)
		loop.AbandonFD(*this);
}

void
SocketEvent::Dispatch() noexcept
{
	const unsigned flags = std::exchange(ready_flags, 0) &
		(GetScheduledFlags() | IMPLICIT_FLAGS);

	if (flags != 0)
		callback(flags);
}
