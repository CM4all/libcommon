// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Response.hxx"

#include <algorithm>
#include <numeric>

#include <assert.h>

namespace Translation::Server {

inline void
Response::Grow(std::size_t new_capacity) noexcept
{
	assert(size <= capacity);
	assert(new_capacity > capacity);

	std::byte *new_buffer = new std::byte[new_capacity];
	std::copy_n(buffer, size, new_buffer);
	delete[] buffer;
	buffer = new_buffer;
	capacity = new_capacity;
}

void *
Response::Write(std::size_t nbytes) noexcept
{
	assert(size <= capacity);

	const std::size_t new_size = size + nbytes;
	if (new_size > capacity)
		Grow(((new_size - 1) | 0x7fff) + 1);

	void *result = buffer + size;
	size = new_size;
	return result;
}

void *
Response::WriteHeader(TranslationCommand cmd, std::size_t payload_size) noexcept
{
	assert(payload_size <= 0xffff);

	const TranslationHeader header{uint16_t(payload_size), cmd};
	void *p = Write(sizeof(header) + payload_size);
	return mempcpy(p, &header, sizeof(header));
}

Response &
Response::Packet(TranslationCommand cmd, std::span<const std::byte> payload) noexcept
{
	void *p = WriteHeader(cmd, payload.size());
	memcpy(p, payload.data(), payload.size());
	return *this;
}

std::span<std::byte>
Response::Finish() noexcept
{
	/* generate a VARY packet? */
	std::size_t n_vary = std::accumulate(vary.begin(), vary.end(), 0,
					     std::plus<std::size_t>{});
	if (n_vary > 0) {
		TranslationCommand *dest = (TranslationCommand *)
			WriteHeader(TranslationCommand::VARY,
				    n_vary * sizeof(*dest));
		for (std::size_t i = 0; i < vary.size(); ++i)
			if (vary[i])
				*dest++ = vary_cmds[i];
	}

	Packet(TranslationCommand::END);

	std::span<std::byte> result{buffer, size};
	buffer = nullptr;
	capacity = size = 0;
	return result;
}

} // namespace Translation::Server
