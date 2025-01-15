// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <mysql.h>

#include <array>
#include <cassert>
#include <string_view>
#include <span>

/* I have split PrepareParamBind and PrepareResultBind, because I don't think the
 * `length = &bind.buffer_length` trick works for result binds.
 */

inline void PrepareParamBind(MYSQL_BIND& bind, std::string_view str)
{
	bind.buffer_type = MYSQL_TYPE_STRING;
	// dcl.type.cv says that any attempt to *modify* const data through non-const pointer
	// is UB. Since mariadb does not attempt to modify the data passed in buffer, so this
	// should be fine.
	bind.buffer = const_cast<char*>(str.data());
	bind.buffer_length = str.size();
	// It's important we do not set buffer.length, because mariadb will set it itself only
	// if it is unset. And it will set it to a pointer to it's own copy of the data,
	// which makes use of temporary MYSQL_BIND objects safe.
}

inline void PrepareParamBind(MYSQL_BIND& bind, long long& v)
{
	// Length is set by mariadb
	bind.buffer_type = MYSQL_TYPE_LONGLONG;
	bind.buffer = &v;
}

inline void PrepareResultBind(MYSQL_BIND& bind, long long& v)
{
	// Length is set by mariadb
	bind.buffer_type = MYSQL_TYPE_LONGLONG;
	bind.buffer = &v;
}

inline void PrepareParamBind(MYSQL_BIND& bind, unsigned long long& v)
{
	// Length is set by mariadb
	bind.buffer_type = MYSQL_TYPE_LONGLONG;
	bind.buffer = &v;
	bind.is_unsigned = true;
}

inline void PrepareResultBind(MYSQL_BIND& bind, unsigned long long& v)
{
	bind.buffer_type = MYSQL_TYPE_LONGLONG;
	bind.buffer = &v;
	bind.is_unsigned = true;
}

template <typename T>
void PrepareResultBind(MYSQL_BIND& bind, T& v)
{
	v.Bind(bind);
}

template <std::size_t N>
struct MysqlParamBind {
	static constexpr auto size = N;
	std::array<MYSQL_BIND, size> binds = {};

	template <typename... Args>
	MysqlParamBind(Args&&... args)
	{
		size_t b = 0;
		// The comma operator always evaluates E1 first in `E1, E2`.
		(PrepareParamBind(binds[b++], std::forward<Args>(args)), ...);
	}

	operator MYSQL_BIND*() { return binds.data(); }
	operator MYSQL_BIND*() const { return binds.data(); }
};

template<typename... Args>
MysqlParamBind(Args&&... args) -> MysqlParamBind<sizeof...(Args)>;

template <std::size_t N>
struct MysqlResultBind {
	static constexpr auto size = N;
	std::array<MYSQL_BIND, size> binds = {};

	template <typename... Args>
	MysqlResultBind(Args&&... args)
	{
		size_t b = 0;
		(PrepareResultBind(binds[b++], std::forward<Args>(args)), ...);
	}

	operator MYSQL_BIND*() { return binds.data(); }
	operator MYSQL_BIND*() const { return binds.data(); }
};

template<typename... Args>
MysqlResultBind(Args&&... args) -> MysqlResultBind<sizeof...(Args)>;

template <std::size_t size>
class MysqlStringBuffer {
	std::array<char, size> value;

	unsigned long length;

public:
	void Bind(MYSQL_BIND& bind) noexcept {
		bind.buffer_type = MYSQL_TYPE_STRING;
		bind.buffer = value.data();
		bind.buffer_length = value.size();
		bind.length = &length;
	}

	constexpr operator std::string_view() const noexcept {
		assert(length <= size);
		return {value.data(), length};
	}
};
