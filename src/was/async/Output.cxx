// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Output.hxx"
#include "net/SocketProtocolError.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <cassert>

namespace Was {

Output::Output(EventLoop &event_loop, UniqueFileDescriptor &&pipe,
	       OutputHandler &_handler) noexcept
	:event(event_loop, BIND_THIS_METHOD(OnPipeReady), pipe.Release()),
	 defer_write(event_loop, BIND_THIS_METHOD(OnDeferredWrite)),
	 handler(_handler)
{
	event.ScheduleImplicit();
}

Output::~Output() noexcept
{
	event.Close();
}

void
Output::Activate(OutputProducer &_producer) noexcept
{
	assert(producer == nullptr);

	producer = &_producer;
	position = 0;

	defer_write.Schedule();
}

void
Output::Deactivate() noexcept
{
	assert(producer != nullptr);

	producer = nullptr;
	CancelWrite();
}

inline void
Output::TryWrite()
{
	assert(producer);

	const std::size_t nbytes = producer->OnWasOutputReady(GetPipe());

	/* if nbytes==0, this object might be deleted already */
	if (nbytes > 0)
		position += nbytes;
}

void
Output::OnPipeReady(unsigned events) noexcept
try {
	if (events & SocketEvent::DEAD_MASK)
		throw SocketClosedPrematurelyError("Hangup on WAS pipe");

	TryWrite();
} catch (...) {
	handler.OnWasOutputError(std::current_exception());
}

inline void
Output::OnDeferredWrite() noexcept
try {
	TryWrite();
} catch (...) {
	handler.OnWasOutputError(std::current_exception());
}

} // namespace Was
