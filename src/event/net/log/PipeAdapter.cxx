// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PipeAdapter.hxx"
#include "event/Loop.hxx"
#include "net/log/Send.hxx"
#include "time/Cast.hxx"

namespace Net {
namespace Log {

bool
PipeAdapter::OnPipeLine(std::span<char> line) noexcept
{
	if (rate_limit > 0) {
		const auto now = GetEventLoop().SteadyNow();
		const auto float_now = ToFloatSeconds(now.time_since_epoch());
		if (!token_bucket.Check(float_now, rate_limit, burst, 1))
			/* rate limit exceeded: discard */
			return true;
	}

	// TODO: erase/quote "dangerous" characters?

	datagram.SetTimestamp(GetEventLoop().SystemNow());

	constexpr size_t MAX_LENGTH = 1024;
	if (line.size() >= MAX_LENGTH) {
		/* truncate long lines */
		line[MAX_LENGTH - 3] = '.';
		line[MAX_LENGTH - 2] = '.';
		line[MAX_LENGTH - 1] = '.';
		line = line.subspan(0, MAX_LENGTH);
	}

	datagram.message = {line.data(), line.size()};

	try {
		Send(socket, datagram);
	} catch (...) {
		// TODO: log this error?
	}

	return true;
}

void
PipeAdapter::OnPipeEnd() noexcept
{
	/* nothing to do here - just wait until this object gets
	   destructed by whoever owns it */
}

}}
