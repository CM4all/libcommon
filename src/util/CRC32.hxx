// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
			state = Update(state, static_cast<uint8_t>(i));
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
