// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Protocol.hxx"

#include <cassert>
#include <cstddef>
#include <span>

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

	std::byte *payload;
	std::size_t payload_position;

public:
	/**
	 * Read a packet from the socket.
	 *
	 * @return the number of bytes consumed
	 */
	std::size_t Feed(AllocatorPtr alloc,
			 std::span<const std::byte> src) noexcept;

	bool IsComplete() const noexcept {
		return state == State::COMPLETE;
	}

	TranslationCommand GetCommand() const noexcept {
		assert(IsComplete());

		return header.command;
	}

	[[gnu::pure]]
	std::span<const std::byte> GetPayload() const noexcept {
		assert(IsComplete());

		return {
			payload != nullptr ? payload : (const std::byte *)"",
			header.length,
		};
	}
};
