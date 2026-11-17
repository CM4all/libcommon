// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Response.hxx"

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

std::byte *
Response::Write(std::size_t nbytes) noexcept
{
	assert(size <= capacity);

	const std::size_t new_size = size + nbytes;
	if (new_size > capacity)
		Grow(((new_size - 1) | 0x7fff) + 1);

	std::byte *result = buffer + size;
	size = new_size;
	return result;
}

std::byte *
Response::WriteHeader(TranslationCommand cmd, std::size_t payload_size) noexcept
{
	if (payload_size > 0xffff) [[unlikely]]
		/* too large for the 16 bit wire length field; remember
		   it and let Finish() fail instead of emitting a header
		   that does not describe the bytes that follow */
		overflow = true;

	const TranslationHeader header{uint16_t(payload_size), cmd};
	const auto src = ReferenceAsBytes(header);
	std::byte *p = Write(sizeof(header) + payload_size);
	return std::copy(src.begin(), src.end(), p);
}

Response &
Response::Packet(TranslationCommand cmd, std::span<const std::byte> payload) noexcept
{
	std::byte *p = WriteHeader(cmd, payload.size());
	std::copy(payload.begin(), payload.end(), p);
	return *this;
}

std::span<std::byte>
Response::Finish() noexcept
{
	if (overflow) [[unlikely]] {
		/* this response had a packet that was too large to be
		   represented */
		delete[] buffer;
		buffer = nullptr;
		capacity = size = 0;
		return {};
	}

	/* generate a VARY packet? */
	std::size_t n_vary = std::accumulate(vary.begin(), vary.end(), 0,
					     std::plus<std::size_t>{});
	if (n_vary > 0) {
		std::byte *dest =
			WriteHeader(TranslationCommand::VARY,
				    n_vary * sizeof(*dest));
		for (std::size_t i = 0; i < vary.size(); ++i) {
			if (vary[i]) {
				const auto src = ReferenceAsBytes(vary_cmds[i]);
				dest = std::copy(src.begin(), src.end(), dest);
			}
		}
	}

	Packet(TranslationCommand::END);

	std::span<std::byte> result{buffer, size};
	buffer = nullptr;
	capacity = size = 0;
	return result;
}

} // namespace Translation::Server
