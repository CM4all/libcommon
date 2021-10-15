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

#include "SimpleInput.hxx"
#include "Buffer.hxx"
#include "Error.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "system/Error.hxx"
#include "util/DisposableBuffer.hxx"

#include <cassert>
#include <cerrno>
#include <stdexcept>

namespace Was {

SimpleInput::SimpleInput(EventLoop &event_loop, UniqueFileDescriptor pipe,
			 SimpleInputHandler &_handler) noexcept
	:event(event_loop, BIND_THIS_METHOD(OnPipeReady), pipe.Release()),
	 defer_read(event_loop, BIND_THIS_METHOD(OnDeferredRead)),
	 handler(_handler)
{
	/* don't schedule READ (until we get an EAGAIN); that would
	   risk receiving a request body before Activate() gets
	   called */
	event.ScheduleImplicit();
}

SimpleInput::~SimpleInput() noexcept
{
	event.Close();
}

void
SimpleInput::Activate() noexcept
{
	assert(!buffer);

	buffer = std::make_unique<Buffer>();

	defer_read.Schedule();
}

bool
SimpleInput::SetLength(std::size_t length) noexcept
{
	if (!buffer || !buffer->SetLength(length))
		return false;

	if (buffer->IsComplete()) {
		event.CancelRead();
		defer_read.Cancel();
	}

	return true;
}

DisposableBuffer
SimpleInput::CheckComplete() noexcept
{
	assert(buffer != nullptr);

	return buffer->IsComplete()
		? buffer.release()->ToDisposableBuffer()
		: nullptr;
}

void
SimpleInput::Premature(std::size_t nbytes)
{
	event.CancelRead();
	defer_read.Cancel();

	if (!buffer) {
		if (nbytes == 0)
			return;
		else
			throw WasProtocolError("Malformed PREMATURE packet");
	}

	const std::size_t fill = buffer->GetFill();
	buffer.reset();
	if (fill > nbytes)
		/* we have already received more data than that, which
		   should not be possible */
		throw WasProtocolError("Too much data on WAS pipe");

	std::size_t discard = nbytes - fill;

	while (discard > 0) {
		char dummy[4096];
		auto n = GetPipe().Read(dummy,
					std::min(sizeof(dummy), discard));
		if (n < 0)
			throw MakeErrno("Read error on WAS pipe");

		if (n == 0)
			throw std::runtime_error("Hangup on WAS pipe");

		discard -= n;
	}
}

void
SimpleInput::TryRead()
{
	assert(buffer);

	auto w = buffer->Write();
	if (w.empty())
		throw std::runtime_error("Unexpected data on WAS pipe");

	auto nbytes = GetPipe().Read(w.data, w.size);
	if (nbytes <= 0) {
		if (nbytes == 0)
			throw std::runtime_error("Hangup on WAS pipe");
		else if (errno == EAGAIN) {
			event.ScheduleRead();
			return;
		} else
			throw MakeErrno("Read error on WAS pipe");
	}

	buffer->Append(nbytes);

	if (buffer->IsComplete()) {
		event.CancelRead();
		defer_read.Cancel();

		handler.OnWasInput(buffer.release()->ToDisposableBuffer());
	}
}

void
SimpleInput::OnPipeReady(unsigned events) noexcept
try {
	if (events & (SocketEvent::HANGUP|SocketEvent::ERROR))
		throw std::runtime_error("Hangup on WAS pipe");

	assert(buffer);

	TryRead();
} catch (...) {
	handler.OnWasInputError(std::current_exception());
}

void
SimpleInput::OnDeferredRead() noexcept
try {
	assert(buffer);

	TryRead();
} catch (...) {
	handler.OnWasInputError(std::current_exception());
}

} // namespace Was
