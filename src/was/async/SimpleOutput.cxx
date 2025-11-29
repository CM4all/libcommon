// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "SimpleOutput.hxx"
#include "Output.hxx"
#include "system/Error.hxx"

#include <cassert>
#include <cerrno>

namespace Was {

void
SimpleOutput::OnWasOutputBegin(Output &_output) noexcept
{
	output = &_output;
}

void
SimpleOutput::OnWasOutputReady()
{
	if (buffer.empty()) {
		output->End();
		return;
	}

	const std::size_t position = output->GetPosition();
	assert(position < buffer.size());

	std::span<const std::byte> r = buffer;
	r = r.subspan(position);
	assert(!r.empty());

	auto nbytes = output->GetPipe().Write(r);
	if (nbytes <= 0) {
		if (nbytes == 0 || errno == EAGAIN) {
			output->ScheduleWrite();
			return;
		} else
			throw MakeErrno("Write error on WAS pipe");
	}

	output->AddPosition(nbytes);

	if (output->GetPosition() == buffer.size()) {
		/* done */
		output->End();
	} else
		output->ScheduleWrite();
}

} // namespace Was
