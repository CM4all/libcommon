// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "BareInetAddress.hxx"
#include "IPv4Address.hxx"
#include "SocketAddress.hxx"
#include "util/IntHash.hxx"

#ifdef HAVE_IPV6
#include "IPv6Address.hxx"
#endif

bool
BareInetAddress::CopyFrom(SocketAddress src) noexcept
{
	if (src.GetFamily() == AF_INET) {
		const auto &v4 = IPv4Address::Cast(src);
		array[0] = 0;
		array[1] = 0;
		array[2] = 0xffff;
		array[3] = v4.GetNumericAddressBE();
		return true;
#ifdef HAVE_IPV6
	} else if (src.GetFamily() == AF_INET6) {
		const auto &v6 = IPv6Address::Cast(src);
		const auto &a = v6.GetAddress();
		array[0] = a.s6_addr32[0];
		array[1] = a.s6_addr32[1];
		array[2] = a.s6_addr32[2];
		array[3] = a.s6_addr32[3];
		return true;
#endif // HAVE_IPV6
	} else
		return false;
}

std::size_t
BareInetAddress::Hash() const noexcept
{
	return IntHash(std::span{array});
}
