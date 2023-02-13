// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <openssl/bio.h>

#include <memory>

struct BIODeleter {
	void operator()(BIO *bio) noexcept {
		BIO_free(bio);
	}
};

using UniqueBIO = std::unique_ptr<BIO, BIODeleter>;
