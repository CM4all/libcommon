// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PReader.hxx"
#include "AllocatorPtr.hxx"

#include <string.h>

std::size_t
TranslatePacketReader::Feed(AllocatorPtr alloc,
			    std::span<const std::byte> src) noexcept
{
	assert(state == State::HEADER ||
	       state == State::PAYLOAD ||
	       state == State::COMPLETE);

	/* discard the packet that was completed (and consumed) by the
	   previous call */
	if (state == State::COMPLETE)
		state = State::HEADER;

	std::size_t consumed = 0;

	if (state == State::HEADER) {
		if (src.size() < sizeof(header))
			/* need more data */
			return 0;

		memcpy(&header, src.data(), sizeof(header));

		if (header.length == 0) {
			payload = nullptr;
			state = State::COMPLETE;
			return sizeof(header);
		}

		consumed += sizeof(header);
		src = src.subspan(sizeof(header));

		state = State::PAYLOAD;

		payload_position = 0;
		payload = alloc.NewArray<std::byte>(header.length + 1);
		payload[header.length] = std::byte{0};

		if (src.empty())
			return consumed;
	}

	assert(state == State::PAYLOAD);

	assert(payload_position < header.length);

	std::size_t nbytes = header.length - payload_position;
	if (nbytes > src.size())
		nbytes = src.size();

	memcpy(payload + payload_position, src.data(), nbytes);
	payload_position += nbytes;
	if (payload_position == header.length)
		state = State::COMPLETE;

	consumed += nbytes;
	return consumed;
}
