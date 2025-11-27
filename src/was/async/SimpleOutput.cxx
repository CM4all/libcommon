// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "SimpleOutput.hxx"
#include "system/Error.hxx"

#include <cassert>
#include <cerrno>

namespace Was {

SimpleOutput::SimpleOutput(EventLoop &event_loop, UniqueFileDescriptor &&pipe,
			   SimpleOutputHandler &_handler) noexcept
	:output(event_loop, std::move(pipe), *this),
	 handler(_handler)
{
}

SimpleOutput::~SimpleOutput() noexcept = default;

void
SimpleOutput::Activate(DisposableBuffer _buffer) noexcept
{
	assert(!buffer);

	if (_buffer.empty())
		return;

	buffer = std::move(_buffer);
	output.Activate(*this);
	output.DeferWrite();
}

void
SimpleOutput::OnWasOutputError(std::exception_ptr &&error) noexcept
{
	handler.OnWasOutputError(std::move(error));
}

std::size_t
SimpleOutput::OnWasOutputReady(FileDescriptor pipe)
{
	assert(buffer);

	std::size_t position = output.GetPosition();
	assert(position < buffer.size());

	std::span<const std::byte> r = buffer;
	r = r.subspan(position);
	assert(!r.empty());

	auto nbytes = pipe.Write(r);
	if (nbytes <= 0) {
		if (nbytes == 0 || errno == EAGAIN) {
			output.ScheduleWrite();
			return 0;
		} else
			throw MakeErrno("Write error on WAS pipe");
	}

	position += nbytes;

	if (position == buffer.size()) {
		/* done */
		output.Deactivate();
	} else
		output.ScheduleWrite();

	return nbytes;
}

} // namespace Was
