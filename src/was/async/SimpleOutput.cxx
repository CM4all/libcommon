/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "SimpleOutput.hxx"
#include "Buffer.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "system/Error.hxx"
#include "util/DisposableBuffer.hxx"

#include <cassert>
#include <cerrno>
#include <stdexcept>

namespace Was {

SimpleOutput::SimpleOutput(EventLoop &event_loop, UniqueFileDescriptor pipe,
			 SimpleOutputHandler &_handler) noexcept
	:event(event_loop, BIND_THIS_METHOD(OnPipeReady), pipe.Release()),
	 defer_write(event_loop, BIND_THIS_METHOD(OnDeferredWrite)),
	 handler(_handler)
{
	event.ScheduleImplicit();
}

SimpleOutput::~SimpleOutput() noexcept
{
	event.Close();
}

void
SimpleOutput::Activate(DisposableBuffer _buffer) noexcept
{
	assert(!buffer);

	if (_buffer.empty())
		return;

	buffer = std::move(_buffer);
	position = 0;

	defer_write.Schedule();
}

void
SimpleOutput::OnPipeReady(unsigned events) noexcept
try {
	if (events & (SocketEvent::HANGUP|SocketEvent::ERROR))
		throw std::runtime_error("Hangup on WAS pipe");

	TryWrite();
} catch (...) {
	handler.OnWasOutputError(std::current_exception());
}

inline void
SimpleOutput::OnDeferredWrite() noexcept
try {
	TryWrite();
} catch (...) {
	handler.OnWasOutputError(std::current_exception());
}

inline void
SimpleOutput::TryWrite()
{
	assert(buffer);
	assert(position < buffer.size());

	std::span<const std::byte> r = buffer;
	r = r.subspan(position);
	assert(!r.empty());

	auto nbytes = GetPipe().Write(r.data(), r.size());
	if (nbytes <= 0) {
		if (nbytes == 0 || errno == EAGAIN) {
			event.ScheduleWrite();
			return;
		} else
			throw MakeErrno("Write error on WAS pipe");
	}

	position += nbytes;

	if (position == buffer.size()) {
		/* done */
		buffer = {};
		event.ScheduleImplicit();
	} else
		event.ScheduleWrite();
}

} // namespace Was
