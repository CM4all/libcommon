// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <openssl/bn.h>

#include <memory>

/**
 * @param clear clear memory before freeing it; use this for sensitive
 * values such as private keys
 */
template<bool clear>
struct BNDeleter {
	void operator()(BIGNUM *bn) noexcept {
		if (clear)
			BN_clear_free(bn);
		else
			BN_free(bn);
	}
};

template<bool clear>
using UniqueBIGNUM = std::unique_ptr<BIGNUM, BNDeleter<clear>>;
