// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Box.hxx"
#include "util/AllocatedArray.hxx"

/**
 * This overload returns the cipher text in an #AllocatedArray.
 */
[[nodiscard]]
static inline AllocatedArray<std::byte>
crypto_box_seal(std::span<const std::byte> m,
		CryptoBoxPublicKeyView pk) noexcept
{
	AllocatedArray<std::byte> c{crypto_box_SEALBYTES + m.size()};
	crypto_box_seal(c.data(), m, pk);
	return c;
}

/**
 * This overload returns the message text in an #AllocatedArray.
 *
 * @return nullptr on error
 */
[[nodiscard]]
static inline AllocatedArray<std::byte>
crypto_box_seal_open(std::span<const std::byte> c,
		     CryptoBoxPublicKeyView pk,
		     CryptoBoxSecretKeyView sk) noexcept
{
	if (c.size() < crypto_box_SEALBYTES)
		return nullptr;

	AllocatedArray<std::byte> m{c.size() - crypto_box_SEALBYTES};
	if (crypto_box_seal_open(m.data(), c, pk, sk) != 0)
		return nullptr;

	return m;
}
