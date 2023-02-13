// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <openssl/ec.h>

#include <memory>

#if OPENSSL_VERSION_NUMBER < 0x30000000L

struct ECDeleter {
	void operator()(EC_KEY *key) noexcept {
		EC_KEY_free(key);
	}
};

using UniqueEC_KEY = std::unique_ptr<EC_KEY, ECDeleter>;

#endif
