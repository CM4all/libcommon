/*
 * Copyright 2007-2020 CM4all GmbH
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

#include "Response.hxx"

#include <algorithm>

#include <assert.h>
#include <string.h>

namespace Translation::Server {

inline void
Response::Grow(size_t new_capacity) noexcept
{
	assert(size <= capacity);
	assert(new_capacity > capacity);

	uint8_t *new_buffer = new uint8_t[new_capacity];
	std::copy_n(buffer, size, new_buffer);
	delete[] buffer;
	buffer = new_buffer;
	capacity = new_capacity;
}

void *
Response::Write(size_t nbytes) noexcept
{
	assert(size <= capacity);

	const size_t new_size = size + nbytes;
	if (new_size > capacity)
		Grow(((new_size - 1) | 0x7fff) + 1);

	void *result = buffer + size;
	size = new_size;
	return result;
}

void *
Response::WriteHeader(TranslationCommand cmd, size_t payload_size) noexcept
{
	assert(payload_size <= 0xffff);

	const TranslationHeader header{uint16_t(payload_size), cmd};
	void *p = Write(sizeof(header) + payload_size);
	return mempcpy(p, &header, sizeof(header));
}

void
Response::Packet(TranslationCommand cmd, ConstBuffer<void> payload) noexcept
{
	void *p = WriteHeader(cmd, payload.size);
	memcpy(p, payload.data, payload.size);
}

} // namespace Translation::Server
