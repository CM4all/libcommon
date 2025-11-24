// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "SHA3.hxx"
#include "EvpDigest.hxx"
#include "util/HexFormat.hxx"

FixedString<64>
EvpSHA3_256_Hex(std::span<const std::byte> input)
{
    const auto digest = EvpDigest<32>(input, EVP_sha3_256());
    return HexFormat(std::span{digest});
}

