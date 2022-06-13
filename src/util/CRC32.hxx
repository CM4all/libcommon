/*
 * Copyright 2021-2022 CM4all GmbH
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

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

/**
 * A naive (and slow) CRC-32/ISO-HDLC implementation.
 */
class CRC32State {
public:
	using value_type = uint32_t;

private:
	value_type state = 0xffffffff;

public:
	constexpr const auto &Update(std::span<const std::byte> b) noexcept {
		for (auto i : b)
			state = Update(state, (uint8_t)i);
		return *this;
	}

	constexpr value_type Finish() const noexcept {
		return ~state;
	}

private:
	static constexpr value_type Update(value_type crc,
					   uint8_t octet) noexcept {
		for (unsigned i = 0; i < 8; i++) {
			uint32_t bit = (octet ^ crc) & 1;
			crc >>= 1;
			if (bit)
				crc ^= 0xedb88320;

			octet >>= 1;
		}

		return crc;
	}
};

constexpr auto
CRC32(std::span<const std::byte> src) noexcept
{
	CRC32State crc;
	crc.Update(src);
	return crc.Finish();
}
