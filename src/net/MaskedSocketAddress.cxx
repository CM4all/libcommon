// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "MaskedSocketAddress.hxx"
#include "IPv4Address.hxx"
#include "IPv6Address.hxx"
#include "Parser.hxx"

#include <string>
#include <stdexcept>

#include <assert.h>
#include <string.h>

[[gnu::pure]]
static bool
operator==(const struct in6_addr &a, const struct in6_addr &b) noexcept
{
	return memcmp(&a, &b, sizeof(a)) == 0;
}

[[gnu::pure]]
static unsigned
MaximumPrefixLength(const SocketAddress address) noexcept
{
	switch (address.GetFamily()) {
	case AF_INET:
		return 32;

	case AF_INET6:
		return 128;

	default:
		/* not applicable */
		return 0;
	}
}

[[gnu::pure]]
static bool
IsValidPrefixLength(const IPv4Address &address,
		    unsigned prefix_length) noexcept
{
	assert(prefix_length <= 32);
	return address.GetNumericAddressBE() ==
		(IPv4Address::MaskFromPrefix(prefix_length) &
		 address).GetNumericAddressBE();
}

[[gnu::pure]]
static bool
IsValidPrefixLength(const IPv6Address &address,
		    unsigned prefix_length) noexcept
{
	assert(prefix_length <= 128);
	return address.GetAddress() ==
		(IPv6Address::MaskFromPrefix(prefix_length) &
		 address).GetAddress();
}

[[gnu::pure]]
static bool
IsValidPrefixLength(const SocketAddress address,
		    unsigned prefix_length) noexcept
{
	switch (address.GetFamily()) {
	case AF_INET:
		return IsValidPrefixLength(IPv4Address::Cast(address),
					   prefix_length);

	case AF_INET6:
		return IsValidPrefixLength(IPv6Address::Cast(address),
					   prefix_length);

	default:
		return false;
	}
}

MaskedSocketAddress::MaskedSocketAddress(const char *s)
{
	std::string buffer;
	const char *slash = nullptr;

	if (*s != '/' && *s != '@') {
		slash = strchr(s, '/');
		if (slash != nullptr)
			s = buffer.assign(s, slash).c_str();
	}

	address = ParseSocketAddress(s, 0, false);
	assert(!address.IsNull());

	const size_t max_prefix_length = MaximumPrefixLength(address);

	if (slash != nullptr) {
		if (max_prefix_length == 0)
			throw std::runtime_error("Prefix not supported for this address family");

		const char *pls = slash + 1;
		char *endptr;
		size_t pl = strtoul(pls, &endptr, 10);
		if (endptr == pls || *endptr != 0)
			throw std::runtime_error("Failed to parse prefix length");

		if (pl > max_prefix_length)
			throw std::runtime_error("Prefix length is too big");

		if (pl < max_prefix_length &&
		    !IsValidPrefixLength(address, pl))
			throw std::runtime_error("Invalid prefix length for this address");

		prefix_length = pl;
	} else
		prefix_length = max_prefix_length;
}

bool
MaskedSocketAddress::Matches(SocketAddress other) const noexcept
{
	if (address.IsNull() || !address.IsDefined() ||
	    other.IsNull() || !other.IsDefined() ||
	    address.GetFamily() != other.GetFamily())
		return false;

	if (address == other)
		return true;

	switch (address.GetFamily()) {
	case AF_INET:
		return (IPv4Address::Cast(other) &
			IPv4Address::MaskFromPrefix(prefix_length)).GetNumericAddressBE()
			== IPv4Address::Cast(address).GetNumericAddressBE();

	case AF_INET6:
		return (IPv6Address::Cast(other) &
			IPv6Address::MaskFromPrefix(prefix_length)).GetAddress()
			== IPv6Address::Cast(address).GetAddress();

	default:
		return false;
	}
}
