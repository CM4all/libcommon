// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "BNAlloc.hxx"
#include "BN.hxx"
#include "Error.hxx"
#include "util/AllocatedArray.hxx"

AllocatedArray<std::byte>
BN_bn2bin(const BIGNUM &bn) noexcept
{
	const std::size_t size = BN_num_bytes(&bn);
	AllocatedArray<std::byte> result{size};
	BN_bn2bin(bn, result.data());
	return result;
}

AllocatedArray<std::byte>
BN_bn2binpad(const BIGNUM &bn, std::size_t size)
{
	AllocatedArray<std::byte> result{size};
	if (BN_bn2binpad(bn, result) < 0)
		throw SslError{"BN_bn2binpad() failed"};

	return result;
}
