/*
 * Copyright (C) 2012-2017 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef IPV6_ADDRESS_HXX
#define IPV6_ADDRESS_HXX

#include "SocketAddress.hxx"
#include "util/ByteOrder.hxx"

#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

/**
 * An OO wrapper for struct sockaddr_in.
 */
class IPv6Address {
	struct sockaddr_in6 address;

	static constexpr struct in6_addr Construct(uint16_t a, uint16_t b,
						   uint16_t c, uint16_t d,
						   uint16_t e, uint16_t f,
						   uint16_t g, uint16_t h) noexcept {
		return {{{
			uint8_t(a >> 8), uint8_t(a),
			uint8_t(b >> 8), uint8_t(b),
			uint8_t(c >> 8), uint8_t(c),
			uint8_t(d >> 8), uint8_t(d),
			uint8_t(e >> 8), uint8_t(e),
			uint8_t(f >> 8), uint8_t(f),
			uint8_t(g >> 8), uint8_t(g),
			uint8_t(h >> 8), uint8_t(h),
		}}};
	}

	static constexpr struct sockaddr_in6 Construct(struct in6_addr address,
						       uint16_t port,
						       uint32_t scope_id) noexcept {
		return {
#if defined(__APPLE__)
			sizeof(struct sockaddr_in6),
#endif
			AF_INET6,
			ToBE16(port),
			0,
			address,
			scope_id,
		};
	}

public:
	IPv6Address() = default;

	constexpr IPv6Address(struct in6_addr _address, uint16_t port,
			      uint32_t scope_id=0) noexcept
		:address(Construct(_address, port, scope_id)) {}

	constexpr explicit IPv6Address(uint16_t port,
				       uint32_t scope_id=0) noexcept
		:IPv6Address(IN6ADDR_ANY_INIT, port, scope_id) {}


	constexpr IPv6Address(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
			      uint16_t e, uint16_t f, uint16_t g, uint16_t h,
			      uint16_t port, uint32_t scope_id=0) noexcept
		:IPv6Address(Construct(a, b, c, d, e, f, g, h),
			     port, scope_id) {}

	/**
	 * Convert a #SocketAddress to a #IPv6Address.  Its address family must be AF_INET.
	 */
	explicit IPv6Address(SocketAddress src) noexcept;

	/**
	 * Generate a (net-)mask with the specified prefix length.
	 */
	static constexpr IPv6Address MaskFromPrefix(unsigned prefix_length) noexcept {
		return IPv6Address(MaskWord(prefix_length, 0),
				   MaskWord(prefix_length, 16),
				   MaskWord(prefix_length, 32),
				   MaskWord(prefix_length, 48),
				   MaskWord(prefix_length, 64),
				   MaskWord(prefix_length, 80),
				   MaskWord(prefix_length, 96),
				   MaskWord(prefix_length, 112),
				   ~uint16_t(0),
				   ~uint32_t(0));
	}

	constexpr operator SocketAddress() const noexcept {
		return SocketAddress(reinterpret_cast<const struct sockaddr *>(&address),
				     sizeof(address));
	}

	constexpr SocketAddress::size_type GetSize() const noexcept {
		return sizeof(address);
	}

	constexpr bool IsDefined() const noexcept {
		return address.sin6_family != AF_UNSPEC;
	}

	constexpr bool IsValid() const noexcept {
		return address.sin6_family == AF_INET6;
	}

	constexpr uint16_t GetPort() const noexcept {
		return FromBE16(address.sin6_port);
	}

	void SetPort(uint16_t port) noexcept {
		address.sin6_port = ToBE16(port);
	}

	constexpr const struct in6_addr &GetAddress() const noexcept {
		return address.sin6_addr;
	}

	constexpr uint32_t GetScopeId() const noexcept {
		return address.sin6_scope_id;
	}

	/**
	 * Is this the IPv6 wildcard address (in6addr_any)?
	 */
	gcc_pure
	bool IsAny() const noexcept;

	/**
	 * Is this an IPv4 address mapped inside struct sockaddr_in6?
	 */
#if !GCC_OLDER_THAN(5,0)
	constexpr
#endif
	bool IsV4Mapped() const noexcept {
		return IN6_IS_ADDR_V4MAPPED(&address.sin6_addr);
	}

private:
	/**
	 * Helper function for MaskFromPrefix().
	 */
	static constexpr uint16_t MaskWord(unsigned prefix_length,
					   unsigned offset) noexcept {
		return prefix_length <= offset
			? 0
			: (prefix_length >= offset + 16
			   ? 0xffff
			   : (0xffff << (offset + 16 - prefix_length)));
	}
};

#endif
