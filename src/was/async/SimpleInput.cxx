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
#include "io/UniqueFileDescriptor.hxx"
#include "system/Error.hxx"
#include "util/DisposableBuffer.hxx"

#include <cassert>
#include <cerrno>
#include <stdexcept>

namespace Was {

SimpleInput::SimpleInput(EventLoop &event_loop, UniqueFileDescriptor pipe,
			 SimpleInputHandler &_handler) noexcept
	:event(event_loop, BIND_THIS_METHOD(OnPipeReady),
	       SocketDescriptor::FromFileDescriptor(pipe.Release())),
	 handler(_handler)
{
	event.ScheduleRead();
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
}

bool
SimpleInput::SetLength(std::size_t length) noexcept
{
	return buffer && buffer->SetLength(length);
}

DisposableBuffer
SimpleInput::CheckComplete() noexcept
{
	assert(buffer != nullptr);

	return buffer->IsComplete()
		? buffer.release()->ToDisposableBuffer()
		: nullptr;
}

bool
SimpleInput::Premature(std::size_t nbytes) noexcept
{
	if (!buffer)
		return nbytes == 0;

	const std::size_t fill = buffer->GetFill();
	buffer.reset();
	if (nbytes > fill)
		return false;

	discard = nbytes - fill;
	return true;
}

void
SimpleInput::OnPipeReady(unsigned events) noexcept
try {
	if (events & (SocketEvent::HANGUP|SocketEvent::ERROR))
		throw std::runtime_error("Hangup on WAS pipe");

	if (discard > 0) {
		char dummy[4096];
		auto nbytes = GetPipe().Read(dummy,
					     std::min(sizeof(dummy), discard));
		if (nbytes < 0)
			throw MakeErrno("Read error on WAS pipe");

		discard -= nbytes;
		return;
	}

	if (!buffer)
		throw std::runtime_error("Unexpected data on WAS pipe");

	auto w = buffer->Write();
	if (w.empty())
		throw std::runtime_error("Unexpected data on WAS pipe");

	auto nbytes = GetPipe().Read(w.data, w.size);
	if (nbytes <= 0) {
		if (nbytes == 0)
			throw std::runtime_error("Hangup on WAS pipe");
		else if (errno == EAGAIN)
			return;
		else
			throw MakeErrno("Read error on WAS pipe");
	}

	buffer->Append(nbytes);

	if (buffer->IsComplete())
		handler.OnWasInput(buffer.release()->ToDisposableBuffer());
} catch (...) {
	handler.OnWasInputError(std::current_exception());
}

} // namespace Was
