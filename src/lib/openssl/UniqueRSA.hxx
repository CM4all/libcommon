// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <openssl/rsa.h>

#include <memory>

#if OPENSSL_VERSION_NUMBER < 0x30000000L

struct RSADeleter {
	void operator()(RSA *rsa) noexcept {
		RSA_free(rsa);
	}
};

using UniqueRSA = std::unique_ptr<RSA, RSADeleter>;

#endif
