// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <openssl/ssl.h>
#include <openssl/bn.h>

#include <span>
#include <utility>

class SslBuffer {
	std::span<unsigned char> span;

public:
	explicit SslBuffer(const X509 &cert);
	explicit SslBuffer(const X509_NAME &cert);
	explicit SslBuffer(const X509_REQ &req);
	explicit SslBuffer(const EVP_PKEY &key);
	explicit SslBuffer(const BIGNUM &bn);

	SslBuffer(SslBuffer &&src) noexcept
		:span(std::exchange(src.span, {}))
	{
	}

	~SslBuffer() noexcept {
		if (span.data() != nullptr)
			OPENSSL_free(span.data());
	}

	SslBuffer &operator=(SslBuffer &&src) noexcept {
		using std::swap;
		swap(span, src.span);
		return *this;
	}

	std::span<const std::byte> get() const noexcept {
		return std::as_bytes(span);
	}
};
