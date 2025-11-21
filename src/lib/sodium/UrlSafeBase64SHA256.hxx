// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Base64Fixed.hxx"
#include "SHA256.hxx"

[[gnu::pure]]
inline auto
UrlSafeBase64SHA256(std::span<const std::byte> src) noexcept
{
	const auto hash = SHA256(src);
	return FixedBase64<sodium_base64_VARIANT_URLSAFE_NO_PADDING>(std::span{hash});
}
