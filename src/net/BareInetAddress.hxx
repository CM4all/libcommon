// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/ByteOrder.hxx"

#include <cstddef>
#include <cstdint>
#include <span>

class SocketAddress;

/**
 * A class that can store either an IPv4 or an IPv6 address.  It is
 * similar to #InetAddress, but stores only the bare IP address, and
 * no port (and no IPv6 scope id).
 */
class BareInetAddress {
	/**
	 * This is effectively an #in6_addr, or a V4-mapped IPv4
	 * address and network byte order.
	 */
	uint32_t array[4];

public:
	/**
	 * Leave the object uninitialized.
	 */
	constexpr BareInetAddress() noexcept = default;

	constexpr bool IsV4Mapped() const noexcept {
		return array[0] == 0 && array[1] == 0 && array[2] == ToBE32(0xffff);
	}

	/**
	 * @return true on success, false the the specified
	 * #SocketAddress is not compatible with this class.
	 */
	[[nodiscard]]
	bool CopyFrom(SocketAddress src) noexcept;

	[[gnu::pure]]
	std::size_t Hash() const noexcept;

	constexpr bool operator==(const BareInetAddress &other) const noexcept = default;

	/**
	 * Parse a C string as a IPv4/IPv6 address into this object.
	 *
	 * @return true on success, false on error
	 */
	bool Parse(const char *s) noexcept;

	/**
	 * Format this object as a C string into the given buffer.
	 *
	 * @return the C string on success, nullptr on error
	 */
	const char *Format(std::span<char> buffer) const noexcept;
};
