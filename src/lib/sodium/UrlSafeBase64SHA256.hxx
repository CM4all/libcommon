// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Base64.hxx"
#include "SHA256.hxx"

template<typename T>
[[gnu::pure]]
inline auto
UrlSafeBase64SHA256(const T &src) noexcept
{
	const auto hash = SHA256(src);
	return FixedBase64<crypto_hash_sha256_BYTES, sodium_base64_VARIANT_URLSAFE_NO_PADDING>(&hash);
}
