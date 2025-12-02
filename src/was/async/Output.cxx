// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Output.hxx"
#include "Producer.hxx"
#include "system/Error.hxx"
#include "net/SocketProtocolError.hxx"
#include "io/UniqueFileDescriptor.hxx"

namespace Was {

Output::Output(EventLoop &event_loop, UniqueFileDescriptor &&pipe,
	       OutputHandler &_handler) noexcept
	:event(event_loop, BIND_THIS_METHOD(OnPipeReady), pipe.Release()),
	 defer_write(event_loop, BIND_THIS_METHOD(OnDeferredWrite)),
	 handler(&_handler)
{
	event.ScheduleImplicit();
}

Output::~Output() noexcept
{
	event.Close();
}

bool
Output::Activate(std::unique_ptr<OutputProducer> &&_producer) noexcept
{
	assert(producer == nullptr);

	producer = std::move(_producer);
	position = 0;

	defer_write.Schedule();

	return producer->OnWasOutputBegin(*this);
}

void
Output::Deactivate() noexcept
{
	assert(producer != nullptr);

#ifndef NDEBUG
	length.reset();
#endif

	producer.reset();
	CancelWrite();
}

std::size_t
Output::Write(std::span<const std::byte> src)
{
	const auto result = GetPipe().Write(src);
	if (result <= 0) {
		if (result == 0)
			return 0;

		const int e = errno;
		if (e == EAGAIN) {
			ScheduleWrite();
			return 0;
		} else
			throw MakeErrno(e, "Write error on WAS pipe");
	}

	const std::size_t nbytes = static_cast<std::size_t>(result);
	if (nbytes < src.size())
		ScheduleWrite();

	AddPosition(nbytes);
	return nbytes;
}

inline void
Output::TryWrite()
{
	assert(producer);

	producer->OnWasOutputReady();
}

void
Output::OnPipeReady(unsigned events) noexcept
try {
	if (events & SocketEvent::DEAD_MASK)
		throw SocketClosedPrematurelyError("Hangup on WAS pipe");

	TryWrite();
} catch (...) {
	handler->OnWasOutputError(std::current_exception());
}

inline void
Output::OnDeferredWrite() noexcept
try {
	TryWrite();
} catch (...) {
	handler->OnWasOutputError(std::current_exception());
}

} // namespace Was
