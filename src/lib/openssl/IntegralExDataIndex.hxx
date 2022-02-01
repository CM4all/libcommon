/*
 * Copyright 2022 CM4all GmbH
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

#include "TrivialExDataIndex.hxx"

#include <cstddef>
#include <type_traits>

namespace OpenSSL {

/**
 * Register an application-specific data index for #SSL objects
 * containing an integral/enum value, defaulting to zero.
 */
template<typename T>
class IntegralExDataIndex {
	static_assert(std::is_integral_v<T> ||
		      std::is_enum_v<T>);
	static_assert(sizeof(T) <= sizeof(void *));

	TrivialExDataIndex idx;

public:
	void Set(SSL &ssl, T value) const noexcept {
		idx.Set(ssl, reinterpret_cast<void *>(static_cast<std::ptrdiff_t>(value)));
	}

	[[gnu::pure]]
	T Get(SSL &ssl) const noexcept {
		return static_cast<T>(reinterpret_cast<std::ptrdiff_t>(idx.Get(ssl)));
	}
};

} // namespace OpenSSL
