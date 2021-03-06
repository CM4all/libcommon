/*
 * Copyright 2020-2021 CM4all GmbH
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

#include "util/ConstBuffer.hxx"

#include <sodium/crypto_generichash.h>

class GenericHashState {
	crypto_generichash_state state;

public:
	explicit GenericHashState(size_t outlen,
				  ConstBuffer<void> key=nullptr) noexcept {
		crypto_generichash_init(&state,
					(const unsigned char *)key.data, key.size, outlen);
	}

	void Update(ConstBuffer<void> p) noexcept {
		crypto_generichash_update(&state,
					  (const unsigned char *)p.data,
					  p.size);
	}

	template<typename T>
	void UpdateT(const T &p) noexcept {
		Update({&p, sizeof(p)});
	}

	void Final(void *out, size_t outlen) noexcept {
		crypto_generichash_final(&state, (unsigned char *)out, outlen);
	}

	template<typename T>
	void FinalT(T &p) noexcept {
		Final(&p, sizeof(p));
	}

	template<typename T>
	[[gnu::pure]]
	T GetFinalT() noexcept {
		T result;
		FinalT(result);
		return result;
	}
};
