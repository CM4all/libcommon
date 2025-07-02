// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "PipeAdapter.hxx"
#include "event/Loop.hxx"
#include "net/log/Sink.hxx"
#include "time/Cast.hxx"

namespace Net::Log {

bool
PipeAdapter::OnPipeLine(std::span<char> line) noexcept
{
	if (rate_limit.rate > 0) {
		const auto now = GetEventLoop().SteadyNow();
		const auto float_now = ToFloatSeconds(now.time_since_epoch());
		if (!token_bucket.Check(rate_limit, float_now, 1))
			/* rate limit exceeded: discard */
			return true;
	}

	// TODO: erase/quote "dangerous" characters?

	datagram.SetTimestamp(GetEventLoop().SystemNow());

	datagram.message = {line.data(), line.size()};

	/* truncate long lines */
	datagram.TruncateMessage(1024);

	sink.Log(datagram);

	return true;
}

void
PipeAdapter::OnPipeEnd() noexcept
{
	/* nothing to do here - just wait until this object gets
	   destructed by whoever owns it */
}

} // namespace Net::Log
