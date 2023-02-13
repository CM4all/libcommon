// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <openssl/ssl.h>

#include <memory>

struct SSLDeleter {
	void operator()(SSL *ssl) noexcept {
		SSL_free(ssl);
	}
};

using UniqueSSL = std::unique_ptr<SSL, SSLDeleter>;
