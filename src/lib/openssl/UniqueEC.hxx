// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <openssl/ec.h>

#include <memory>

struct ECDeleter {
#if OPENSSL_API_LEVEL < 30000
	void operator()(EC_KEY *key) noexcept {
		EC_KEY_free(key);
	}
#endif

	void operator()(EC_GROUP *group) noexcept {
		EC_GROUP_free(group);
	}

	void operator()(EC_POINT *p) noexcept {
		EC_POINT_free(p);
	}

	void operator()(ECDSA_SIG *sig) noexcept {
		ECDSA_SIG_free(sig);
	}
};

#if OPENSSL_API_LEVEL < 30000
using UniqueEC_KEY = std::unique_ptr<EC_KEY, ECDeleter>;
#endif

using UniqueEC_GROUP = std::unique_ptr<EC_GROUP, ECDeleter>;
using UniqueEC_POINT = std::unique_ptr<EC_POINT, ECDeleter>;
using UniqueECDSA_SIG = std::unique_ptr<ECDSA_SIG, ECDeleter>;
