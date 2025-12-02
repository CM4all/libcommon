// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "SimpleOutput.hxx"
#include "Output.hxx"

#include <cassert>

namespace Was {

bool
SimpleOutput::OnWasOutputBegin(Output &_output) noexcept
{
	output = &_output;
	return output->SetLength(buffer.size());
}

void
SimpleOutput::OnWasOutputReady()
{
	if (buffer.empty()) {
		output->End();
		return;
	}

	const uint_least64_t position = output->GetPosition();
	assert(position < buffer.size());

	std::span<const std::byte> r = buffer;
	r = r.subspan(position);
	assert(!r.empty());

	const std::size_t nbytes = output->Write(r);

	if (nbytes == r.size())
		/* done */
		output->End();
}

} // namespace Was
