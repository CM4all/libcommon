// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
