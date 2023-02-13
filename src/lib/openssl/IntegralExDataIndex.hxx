// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
requires std::is_integral_v<T> || std::is_enum_v<T>
class IntegralExDataIndex {
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
