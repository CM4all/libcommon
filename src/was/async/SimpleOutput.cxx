// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "SimpleOutput.hxx"
#include "system/Error.hxx"

#include <cassert>
#include <cerrno>

namespace Was {

SimpleOutput::SimpleOutput(Output &_output,
			   SimpleOutputHandler &_handler) noexcept
	:output(_output),
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
}

void
SimpleOutput::OnWasOutputReady()
{
	assert(buffer);

	const std::size_t position = output.GetPosition();
	assert(position < buffer.size());

	std::span<const std::byte> r = buffer;
	r = r.subspan(position);
	assert(!r.empty());

	auto nbytes = output.GetPipe().Write(r);
	if (nbytes <= 0) {
		if (nbytes == 0 || errno == EAGAIN) {
			output.ScheduleWrite();
			return;
		} else
			throw MakeErrno("Write error on WAS pipe");
	}

	output.AddPosition(nbytes);

	if (output.GetPosition() == buffer.size()) {
		/* done */
		buffer = {};
		handler.OnWasOutputEnd();
	} else
		output.ScheduleWrite();
}

} // namespace Was
