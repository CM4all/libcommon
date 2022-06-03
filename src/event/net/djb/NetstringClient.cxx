/*
 * Copyright 2007-2022 CM4all GmbH
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

#include "NetstringClient.hxx"
#include "util/ConstBuffer.hxx"

#include <stdexcept>

static constexpr auto send_timeout = std::chrono::seconds(10);
static constexpr auto recv_timeout = std::chrono::minutes(1);
static constexpr auto busy_timeout = std::chrono::seconds(5);

NetstringClient::NetstringClient(EventLoop &event_loop, size_t max_size,
				 NetstringClientHandler &_handler) noexcept
	:event(event_loop, BIND_THIS_METHOD(OnEvent)),
	 timeout_event(event_loop, BIND_THIS_METHOD(OnTimeout)),
	 input(max_size), handler(_handler) {}

NetstringClient::~NetstringClient() noexcept
{
	if (out_fd.IsDefined() || in_fd.IsDefined())
		event.Cancel();

	const bool close_in_fd = in_fd.IsDefined() && in_fd != out_fd;

	if (out_fd.IsDefined())
		out_fd.Close();

	if (close_in_fd)
		in_fd.Close();
}

void
NetstringClient::Request(FileDescriptor _out_fd, FileDescriptor _in_fd,
			 std::list<ConstBuffer<void>> &&data) noexcept
{
	assert(!in_fd.IsDefined());
	assert(!out_fd.IsDefined());
	assert(_in_fd.IsDefined());
	assert(_out_fd.IsDefined());

	out_fd = _out_fd;
	in_fd = _in_fd;

	generator(data);
	for (const auto &i : data)
		write.Push(i.data, i.size);

	event.Open(SocketDescriptor::FromFileDescriptor(out_fd));
	event.ScheduleWrite();
	timeout_event.Schedule(send_timeout);
}

void
NetstringClient::OnEvent(unsigned events) noexcept
try {
	if (events & SocketEvent::WRITE) {
		switch (write.Write(out_fd)) {
		case MultiWriteBuffer::Result::MORE:
			timeout_event.Schedule(send_timeout);
			break;

		case MultiWriteBuffer::Result::FINISHED:
			event.ReleaseSocket();
			event.Open(SocketDescriptor::FromFileDescriptor(in_fd));
			event.ScheduleRead();
			timeout_event.Schedule(recv_timeout);
			break;
		}
	} else if (events & SocketEvent::READ) {
		switch (input.Receive(in_fd)) {
		case NetstringInput::Result::MORE:
			timeout_event.Schedule(busy_timeout);
			break;

		case NetstringInput::Result::CLOSED:
			throw std::runtime_error("Connection closed prematurely");

		case NetstringInput::Result::FINISHED:
			event.Cancel();
			timeout_event.Cancel();
			handler.OnNetstringResponse(std::move(input.GetValue()));
			break;
		}
	}
} catch (...) {
	handler.OnNetstringError(std::current_exception());
}

void
NetstringClient::OnTimeout() noexcept
{
	handler.OnNetstringError(std::make_exception_ptr(std::runtime_error("Connect timeout")));
}
