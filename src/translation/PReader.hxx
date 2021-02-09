/*
 * Copyright 2007-2021 CM4all GmbH
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

#ifndef BENG_PROXY_TRANSLATE_READER_HXX
#define BENG_PROXY_TRANSLATE_READER_HXX

#include "Protocol.hxx"
#include "util/ConstBuffer.hxx"

#include <cassert>
#include <cstddef>
#include <cstdint>

class AllocatorPtr;

/**
 * Parse translation response packets.
 */
class TranslatePacketReader {
	enum class State {
		HEADER,
		PAYLOAD,
		COMPLETE,
	};

	State state = State::HEADER;

	TranslationHeader header;

	char *payload;
	std::size_t payload_position;

public:
	/**
	 * Read a packet from the socket.
	 *
	 * @return the number of bytes consumed
	 */
	std::size_t Feed(AllocatorPtr alloc,
			 const uint8_t *data, std::size_t length);

	bool IsComplete() const {
		return state == State::COMPLETE;
	}

	TranslationCommand GetCommand() const {
		assert(IsComplete());

		return header.command;
	}

	[[gnu::pure]]
	ConstBuffer<void> GetPayload() const noexcept {
		assert(IsComplete());

		return {payload != nullptr ? payload : "", header.length};
	}
};

#endif
