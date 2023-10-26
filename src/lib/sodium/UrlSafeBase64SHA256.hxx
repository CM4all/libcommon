// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Base64.hxx"
#include "SHA256.hxx"

[[gnu::pure]]
inline auto
UrlSafeBase64SHA256(std::span<const std::byte> src) noexcept
{
	const auto hash = SHA256(src);
	return FixedBase64<crypto_hash_sha256_BYTES, sodium_base64_VARIANT_URLSAFE_NO_PADDING>(&hash);
}
