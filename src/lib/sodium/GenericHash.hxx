// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
