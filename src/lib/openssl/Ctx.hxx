// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

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
	SslCtx() noexcept = default;

	explicit SslCtx(const SSL_METHOD *meth)
		:ssl_ctx(SSL_CTX_new(meth)) {
		if (ssl_ctx == nullptr)
			throw SslError("SSL_CTX_new() failed");
	}

	SslCtx(const SslCtx &src) noexcept
		:ssl_ctx(src.ssl_ctx) {
		SSL_CTX_up_ref(ssl_ctx);
	}

	SslCtx(SslCtx &&src) noexcept
		:ssl_ctx(std::exchange(src.ssl_ctx, nullptr)) {}

	~SslCtx() noexcept {
		if (ssl_ctx != nullptr)
			SSL_CTX_free(ssl_ctx);
	}

	SslCtx &operator=(const SslCtx &src) noexcept {
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

	SslCtx &operator=(SslCtx &&src) noexcept {
		std::swap(ssl_ctx, src.ssl_ctx);
		return *this;
	}

	operator bool() const noexcept {
		return ssl_ctx != nullptr;
	}

	SSL_CTX *get() const noexcept {
		return ssl_ctx;
	}

	SSL_CTX &operator*() const noexcept {
		return *ssl_ctx;
	}

	void reset() noexcept {
		if (ssl_ctx != nullptr)
			SSL_CTX_free(ssl_ctx);
		ssl_ctx = nullptr;
	}
};

#endif
