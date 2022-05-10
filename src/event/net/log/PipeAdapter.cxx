/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
