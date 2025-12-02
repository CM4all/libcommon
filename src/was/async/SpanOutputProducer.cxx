// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "SpanOutputProducer.hxx"
#include "Output.hxx"

#include <cassert>

namespace Was {

bool
SpanOutputProducer::OnWasOutputBegin(Output &_output) noexcept
{
	output = &_output;
	return output->SetLength(buffer.size());
}

void
SpanOutputProducer::OnWasOutputReady()
{
	if (buffer.empty()) {
		output->End();
		return;
	}

	const uint_least64_t position = output->GetPosition();
	assert(position < buffer.size());

	const auto r = buffer.subspan(position);
	assert(!r.empty());

	const std::size_t nbytes = output->Write(r);

	if (nbytes == r.size())
		/* done */
		output->End();
}

} // namespace Was
