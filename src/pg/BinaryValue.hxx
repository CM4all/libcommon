// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstddef>
#include <span>

namespace Pg {

struct BinaryValue : std::span<const std::byte> {
	using std::span<const std::byte>::span;

	constexpr BinaryValue(const std::span<const std::byte> src) noexcept
		:std::span<const std::byte>(src) {}

	constexpr BinaryValue(const void *_data, std::size_t _size) noexcept
		:std::span<const std::byte>((const std::byte *)_data, _size) {}

	[[gnu::pure]]
	bool ToBool() const noexcept {
		return size() == 1 && data() != nullptr && *(const bool *)data();
	}
};

} /* namespace Pg */
