// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "SHA1.hxx"
#include "EvpDigest.hxx"
#include "util/HexFormat.hxx"

std::array<char, 40>
EvpSHA1_Hex(std::span<const std::byte> input)
{
    const auto digest = EvpDigest<20>(input, EVP_sha1());
    return HexFormat(std::span{digest});
}
