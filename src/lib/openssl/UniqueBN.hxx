// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <openssl/bn.h>

#include <memory>

struct BNDeleter {
	void operator()(BIGNUM *bn) noexcept {
		BN_free(bn);
	}
};

using UniqueBIGNUM = std::unique_ptr<BIGNUM, BNDeleter>;
