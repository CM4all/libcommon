// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>
#include <cstdint>

class SocketAddress;

/**
 * A class that can store either an IPv4 or an IPv6 address.  It is
 * similar to #InetAddress, but stores only the bare IP address, and
 * no port.
 */
class BareInetAddress {
	uint32_t array[4];

public:
	/**
	 * Leave the object uninitialized.
	 */
	constexpr BareInetAddress() noexcept = default;

	/**
	 * @return true on success, false the the specified
	 * #SocketAddress is not compatible with this class.
	 */
	[[nodiscard]]
	bool CopyFrom(SocketAddress src) noexcept;

	[[gnu::pure]]
	std::size_t Hash() const noexcept;

	constexpr bool operator==(const BareInetAddress &other) const noexcept = default;
};
