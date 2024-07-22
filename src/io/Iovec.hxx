// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <span>

#include <sys/uio.h>

constexpr struct iovec
MakeIovec(std::span<const std::byte> s) noexcept
{
	return { const_cast<std::byte *>(s.data()), s.size() };
}

template<typename T>
constexpr struct iovec
MakeIovec(std::span<T> s) noexcept
{
	return MakeIovec(std::as_bytes(s));
}

template<typename T>
constexpr struct iovec
MakeIovecT(T &t) noexcept
{
	return MakeIovec(std::span{&t, 1});
}

template<typename T, T _value>
inline auto
MakeIovecStatic() noexcept
{
	static constexpr T value = _value;
	return MakeIovecT<const T>(value);
}

constexpr std::span<std::byte>
ToSpan(const struct iovec &i) noexcept
{
	return {static_cast<std::byte *>(i.iov_base), i.iov_len};
}
