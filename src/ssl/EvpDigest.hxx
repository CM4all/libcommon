/*
 * Copyright 2021 CM4all GmbH
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

#include "Error.hxx"

#include <openssl/evp.h>

#include <cassert>
#include <span>
#include <string_view>
#include <utility>

class EvpDigestContext {
	EVP_MD_CTX *ctx = nullptr;

public:
	EvpDigestContext() noexcept = default;

	explicit EvpDigestContext(const EVP_MD *type, ENGINE *impl=nullptr)
		:ctx(EVP_MD_CTX_new())
	{
		EVP_DigestInit_ex(ctx, type, impl);
	}

	EvpDigestContext(EvpDigestContext &&src) noexcept
		:ctx(std::exchange(src.ctx, nullptr)) {}

	~EvpDigestContext() noexcept {
		if (ctx != nullptr)
			EVP_MD_CTX_free(ctx);
	}

	auto &operator=(EvpDigestContext &&src) noexcept {
		using std::swap;
		swap(ctx, src.ctx);
		return *this;
	}

	operator bool() const noexcept {
		return ctx != nullptr;
	}

	void Reset() noexcept {
		assert(ctx != nullptr);

		EVP_MD_CTX_reset(ctx);
	}

	void Update(std::span<const std::byte> input) noexcept {
		assert(ctx != nullptr);

		EVP_DigestUpdate(ctx, input.data(), input.size());
	}

	template<typename T>
	void Update(std::span<T> input) noexcept {
		assert(ctx != nullptr);

		EVP_DigestUpdate(ctx, input.data(), input.size_bytes());
	}

	template<typename T>
	void Update(std::basic_string_view<T> input) noexcept {
		Update(std::span{input.data(), input.size()});
	}

	void Final(unsigned char *md, unsigned int *s) noexcept {
		assert(ctx != nullptr);

		EVP_DigestFinal_ex(ctx, md, s);
	}

	template<std::size_t size>
	auto Final() noexcept {
		std::array<std::byte, size> result;
		Final((unsigned char *)result.data(), nullptr);
		return result;
	}
};

template<const EVP_MD *(*type_function)(), std::size_t size>
class TEvpDigestContext : EvpDigestContext {
public:
	TEvpDigestContext()
		:EvpDigestContext(type_function()) {}

	using EvpDigestContext::Reset;
	using EvpDigestContext::Update;

	auto Final() noexcept {
		return EvpDigestContext::Final<size>();
	}
};

using EvpSHA1Context = TEvpDigestContext<EVP_sha1, 20>;
using EvpSHA3_256Context = TEvpDigestContext<EVP_sha3_256, 32>;

template<std::size_t size, typename T>
auto
EvpDigest(std::span<T> input, const EVP_MD *type, ENGINE *impl=nullptr)
{
	assert(EVP_MD_size(type) == size);

	std::array<std::byte, size> digest;
	if (!EVP_Digest(input.data(), input.size_bytes(),
			(unsigned char *)digest.data(), nullptr,
			type, impl))
		throw SslError("EVP_Digest() failed");

	return digest;
}

template<std::size_t size, typename T>
auto
EvpDigest(std::basic_string_view<T> input,
	  const EVP_MD *type, ENGINE *impl=nullptr)
{
	return EvpDigest<size>(std::span{input.data(), input.size()},
			       type, impl);
}
