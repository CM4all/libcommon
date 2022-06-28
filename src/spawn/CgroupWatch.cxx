/*
 * Copyright 2017-2022 CM4all GmbH
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

#include "CgroupWatch.hxx"
#include "CgroupState.hxx"
#include "io/Open.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"

#include <stdlib.h>

static UniqueFileDescriptor
OpenMemoryUsage(const CgroupState &state)
{
	if (!state.memory_v2)
		throw std::runtime_error("Cgroup2 controller 'memory' not found");

	const auto group = state.GetUnifiedGroupMount();
	if (!group.IsDefined())
		throw std::runtime_error("Cgroup controller 'memory' not found");

	return OpenReadOnly(group, "memory.current");
}

static uint64_t
ReadUint64(FileDescriptor fd)
{
	char buffer[64];
	ssize_t nbytes = pread(fd.Get(), buffer, sizeof(buffer), 0);
	if (nbytes < 0)
		throw MakeErrno("Failed to read cgroup file");

	if ((size_t)nbytes >= sizeof(buffer))
		throw std::runtime_error("Cgroup file is too large");

	char *endptr;
	uint64_t value = strtoull(buffer, &endptr, 10);
	if (endptr == buffer)
		throw std::runtime_error("Failed to parse cgroup file");

	return value;
}

CgroupMemoryWatch::CgroupMemoryWatch(EventLoop &event_loop,
				     const CgroupState &state,
				     uint64_t _threshold,
				     BoundMethod<void(uint64_t value) noexcept> _callback)
	:fd(OpenMemoryUsage(state)),
	 threshold(_threshold),
	 timer(event_loop, BIND_THIS_METHOD(OnTimer)),
	 callback(_callback)
{
	timer.Schedule(std::chrono::minutes(1));
}

void
CgroupMemoryWatch::OnTimer() noexcept
{
	try {
		uint64_t value = ReadUint64(fd);
		if (value >= threshold) {
			callback(value);
			timer.Schedule(std::chrono::seconds(10));
		} else
			timer.Schedule(std::chrono::minutes(1));
	} catch (...) {
		PrintException(std::current_exception());
		timer.Schedule(std::chrono::minutes(5));
	}
}
