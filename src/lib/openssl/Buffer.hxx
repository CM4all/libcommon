/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <openssl/ssl.h>
#include <openssl/bn.h>

#include <span>
#include <utility>

class SslBuffer {
	std::span<unsigned char> span;

public:
	explicit SslBuffer(X509 &cert);
	explicit SslBuffer(X509_NAME &cert);
	explicit SslBuffer(X509_REQ &req);
	explicit SslBuffer(EVP_PKEY &key);
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
