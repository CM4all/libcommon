/*
 * Copyright 2007-2021 CM4all GmbH
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

#ifndef SSL_CTX_HXX
#define SSL_CTX_HXX

#include "Error.hxx"

#include <openssl/ssl.h>

#include <utility>

/**
 * A wrapper for SSL_CTX which takes advantage of OpenSSL's reference
 * counting.
 */
class SslCtx {
	SSL_CTX *ssl_ctx = nullptr;

public:
	SslCtx() = default;

	explicit SslCtx(const SSL_METHOD *meth)
		:ssl_ctx(SSL_CTX_new(meth)) {
		if (ssl_ctx == nullptr)
			throw SslError("SSL_CTX_new() failed");
	}

	SslCtx(const SslCtx &src)
		:ssl_ctx(src.ssl_ctx) {
		SSL_CTX_up_ref(ssl_ctx);
	}

	SslCtx(SslCtx &&src)
		:ssl_ctx(std::exchange(src.ssl_ctx, nullptr)) {}

	~SslCtx() {
		if (ssl_ctx != nullptr)
			SSL_CTX_free(ssl_ctx);
	}

	SslCtx &operator=(const SslCtx &src) {
		/* this check is important because it protects against
		   self-assignment */
		if (ssl_ctx != src.ssl_ctx) {
			if (ssl_ctx != nullptr)
				SSL_CTX_free(ssl_ctx);

			ssl_ctx = src.ssl_ctx;
			if (ssl_ctx != nullptr)
				SSL_CTX_up_ref(ssl_ctx);
		}

		return *this;
	}

	SslCtx &operator=(SslCtx &&src) {
		std::swap(ssl_ctx, src.ssl_ctx);
		return *this;
	}

	operator bool() const {
		return ssl_ctx != nullptr;
	}

	SSL_CTX *get() const {
		return ssl_ctx;
	}

	SSL_CTX &operator*() const {
		return *ssl_ctx;
	}

	void reset() {
		if (ssl_ctx != nullptr)
			SSL_CTX_free(ssl_ctx);
		ssl_ctx = nullptr;
	}
};

#endif
