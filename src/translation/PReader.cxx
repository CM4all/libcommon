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

#include "PReader.hxx"
#include "AllocatorPtr.hxx"

#include <string.h>

std::size_t
TranslatePacketReader::Feed(AllocatorPtr alloc,
			    const std::byte *data, std::size_t length) noexcept
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
		if (length < sizeof(header))
			/* need more data */
			return 0;

		memcpy(&header, data, sizeof(header));

		if (header.length == 0) {
			payload = nullptr;
			state = State::COMPLETE;
			return sizeof(header);
		}

		consumed += sizeof(header);
		data += sizeof(header);
		length -= sizeof(header);

		state = State::PAYLOAD;

		payload_position = 0;
		payload = alloc.NewArray<std::byte>(header.length + 1);
		payload[header.length] = std::byte{0};

		if (length == 0)
			return consumed;
	}

	assert(state == State::PAYLOAD);

	assert(payload_position < header.length);

	std::size_t nbytes = header.length - payload_position;
	if (nbytes > length)
		nbytes = length;

	memcpy(payload + payload_position, data, nbytes);
	payload_position += nbytes;
	if (payload_position == header.length)
		state = State::COMPLETE;

	consumed += nbytes;
	return consumed;
}
