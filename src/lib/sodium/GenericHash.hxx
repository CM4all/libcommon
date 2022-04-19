/*
 * Copyright 2020-2022 CM4all GmbH
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

#include <sodium/crypto_generichash.h>

#include <span>
#include <string_view>

class GenericHashState {
	crypto_generichash_state state;

public:
	explicit GenericHashState(size_t outlen,
				  std::span<const std::byte> key={}) noexcept {
		crypto_generichash_init(&state,
					(const unsigned char *)key.data(),
					key.size(), outlen);
	}

	void Update(std::span<const std::byte> p) noexcept {
		crypto_generichash_update(&state,
					  (const unsigned char *)p.data(),
					  p.size());
	}

	template<typename T>
	void Update(std::span<const T> src) noexcept {
		Update(std::as_bytes(src));
	}

	void Update(std::string_view src) noexcept {
		Update(std::span<const char>{src});
	}

	template<typename T>
	void UpdateT(const T &p) noexcept {
		const std::span<const T> span{&p, 1};
		Update(span);
	}

	void Final(std::span<std::byte> out) noexcept {
		crypto_generichash_final(&state, (unsigned char *)out.data(),
					 out.size());
	}

	template<typename T>
	void FinalT(T &p) noexcept {
		const std::span<T> span{&p, 1};
		Final(std::as_writable_bytes(span));
	}

	template<typename T>
	[[gnu::pure]]
	T GetFinalT() noexcept {
		T result;
		FinalT(result);
		return result;
	}
};
