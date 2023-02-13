// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <openssl/ossl_typ.h>

namespace OpenSSL {

/**
 * Register an application-specific data index for #SSL objects
 * containing a pointer with default new/dup/free functions.
 */
class TrivialExDataIndex {
	const int idx;

public:
	TrivialExDataIndex();

	TrivialExDataIndex(const TrivialExDataIndex &) = delete;
	TrivialExDataIndex &operator=(const TrivialExDataIndex &) = delete;

	void Set(SSL &ssl, void *value) const noexcept;

	[[gnu::pure]]
	void *Get(SSL &ssl) const noexcept;
};

} // namespace OpenSSL
