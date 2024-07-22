// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <span>
#include <type_traits> // for std::has_unique_object_representations_v

#include <sys/uio.h>

constexpr struct iovec
MakeIovec(std::span<const std::byte> s) noexcept
{
	return { const_cast<std::byte *>(s.data()), s.size() };
}

template<typename T>
requires std::has_unique_object_representations_v<T>
constexpr struct iovec
MakeIovec(std::span<T> s) noexcept
{
	return MakeIovec(std::as_bytes(s));
}

template<typename T>
requires std::has_unique_object_representations_v<T>
constexpr struct iovec
MakeIovecT(T &t) noexcept
{
	return MakeIovec(std::span{&t, 1});
}

template<typename T, T _value>
requires std::has_unique_object_representations_v<T>
inline auto
MakeIovecStatic() noexcept
{
	static constexpr T value = _value;
	return MakeIovecT(value);
}

constexpr std::span<std::byte>
ToSpan(const struct iovec &i) noexcept
{
	return {static_cast<std::byte *>(i.iov_base), i.iov_len};
}
