// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Curve25519Types.hxx"

#include <sodium/crypto_scalarmult_curve25519.h>

static_assert(sizeof(Curve25519) == crypto_scalarmult_curve25519_BYTES);
static_assert(sizeof(Curve25519Scalar) == crypto_scalarmult_curve25519_SCALARBYTES);

[[nodiscard]]
static inline int
crypto_scalarmult_curve25519(Curve25519Ptr q,
			     Curve25519ScalarView n,
			     Curve25519View p) noexcept
{
	return crypto_scalarmult_curve25519(reinterpret_cast<unsigned char *>(q.data()),
					    reinterpret_cast<const unsigned char *>(n.data()),
					    reinterpret_cast<const unsigned char *>(p.data()));
}

[[nodiscard]]
static inline int
crypto_scalarmult_curve25519_base(Curve25519Ptr q,
				  Curve25519ScalarView n) noexcept
{
	return crypto_scalarmult_curve25519_base(reinterpret_cast<unsigned char *>(q.data()),
						 reinterpret_cast<const unsigned char *>(n.data()));
}
