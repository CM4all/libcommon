// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <openssl/evp.h>

#include <memory>

struct EVPDeleter {
	void operator()(EVP_PKEY *key) noexcept {
		EVP_PKEY_free(key);
	}

	void operator()(EVP_PKEY_CTX *key) noexcept {
		EVP_PKEY_CTX_free(key);
	}

	void operator()(EVP_CIPHER_CTX *key) noexcept {
		EVP_CIPHER_CTX_free(key);
	}
};

using UniqueEVP_PKEY = std::unique_ptr<EVP_PKEY, EVPDeleter>;
using UniqueEVP_PKEY_CTX = std::unique_ptr<EVP_PKEY_CTX, EVPDeleter>;
using UniqueEVP_CIPHER_CTX = std::unique_ptr<EVP_CIPHER_CTX, EVPDeleter>;

static inline auto
UpRef(EVP_PKEY &key) noexcept
{
	EVP_PKEY_up_ref(&key);
	return UniqueEVP_PKEY(&key);
}
