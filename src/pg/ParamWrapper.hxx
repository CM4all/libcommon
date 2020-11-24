/*
 * Copyright 2007-2020 CM4all GmbH
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

#include "Serial.hxx"
#include "BinaryValue.hxx"
#include "Array.hxx"

#include <iterator>
#include <list>
#include <cinttypes>
#include <cstdio>
#include <cstddef>

#if __cplusplus >= 201703L && !GCC_OLDER_THAN(7,0)
#include <string_view>
#endif

namespace Pg {

template<typename T, typename Enable=void>
struct ParamWrapper {
	ParamWrapper(const T &t) noexcept;
	const char *GetValue() const noexcept;

	/**
	 * Is the buffer returned by GetValue() binary?  If so, the method
	 * GetSize() must return the size of the value.
	 */
	bool IsBinary() const noexcept;

	/**
	 * Returns the size of the value in bytes.  Only applicable if
	 * IsBinary() returns true and the value is non-nullptr.
	 */
	size_t GetSize() const noexcept;
};

template<>
struct ParamWrapper<Serial> {
	char buffer[16];

	ParamWrapper(Serial s) noexcept {
		sprintf(buffer, "%" PRIpgserial, s.get());
	}

	const char *GetValue() const noexcept {
		return buffer;
	}

	static constexpr bool IsBinary() noexcept {
		return false;
	}

	size_t GetSize() const noexcept {
		/* ignored for text columns */
		return 0;
	}
};

template<>
struct ParamWrapper<BinaryValue> {
	BinaryValue value;

	constexpr ParamWrapper(BinaryValue _value) noexcept
		:value(_value) {}

	constexpr const char *GetValue() const noexcept {
		return (const char *)value.data;
	}

	static constexpr bool IsBinary() noexcept {
		return true;
	}

	constexpr size_t GetSize() const noexcept {
		return value.size;
	}
};

template<>
struct ParamWrapper<const char *> {
	const char *value;

	constexpr ParamWrapper(const char *_value) noexcept
		:value(_value) {}

	constexpr const char *GetValue() const noexcept {
		return value;
	}

	static constexpr bool IsBinary() noexcept {
		return false;
	}

	size_t GetSize() const noexcept {
		/* ignored for text columns */
		return 0;
	}
};

/* this alias exists only to allow non-const char arrays to be used */
template<>
struct ParamWrapper<char *> : ParamWrapper<const char *> {
	using ParamWrapper<const char *>::ParamWrapper;
};

template<>
struct ParamWrapper<int> {
	char buffer[16];

	ParamWrapper(int i) noexcept {
		sprintf(buffer, "%i", i);
	}

	const char *GetValue() const noexcept {
		return buffer;
	}

	static constexpr bool IsBinary() noexcept {
		return false;
	}

	size_t GetSize() const noexcept {
		/* ignored for text columns */
		return 0;
	}
};

template<>
struct ParamWrapper<int64_t> {
	char buffer[32];

	ParamWrapper(int64_t i) noexcept {
		sprintf(buffer, "%" PRId64, i);
	}

	const char *GetValue() const noexcept {
		return buffer;
	}

	static constexpr bool IsBinary() noexcept {
		return false;
	}

	size_t GetSize() const noexcept {
		/* ignored for text columns */
		return 0;
	}
};

template<>
struct ParamWrapper<unsigned> {
	char buffer[16];

	ParamWrapper(unsigned i) noexcept {
		sprintf(buffer, "%u", i);
	}

	const char *GetValue() const noexcept {
		return buffer;
	}

	static constexpr bool IsBinary() noexcept {
		return false;
	}

	size_t GetSize() const noexcept {
		/* ignored for text columns */
		return 0;
	}
};

template<>
struct ParamWrapper<bool> {
	const char *value;

	constexpr ParamWrapper(bool _value) noexcept
		:value(_value ? "t" : "f") {}

	static constexpr bool IsBinary() noexcept {
		return false;
	}

	constexpr const char *GetValue() const noexcept {
		return value;
	}

	size_t GetSize() const noexcept {
		/* ignored for text columns */
		return 0;
	}
};

#if __cplusplus >= 201703L && !GCC_OLDER_THAN(7,0)

template<>
struct ParamWrapper<std::string_view> {
	std::string_view value;

	constexpr ParamWrapper(std::string_view _value) noexcept
		:value(_value) {}

	static constexpr bool IsBinary() noexcept {
		/* since std::string_view is not null-terminated, we
		   need to pass it as a binary value */
		return true;
	}

	constexpr const char *GetValue() const noexcept {
		return value.data();
	}

	constexpr size_t GetSize() const noexcept {
		return value.size();
	}
};

#endif

/**
 * Specialization for STL container types of std::string instances.
 */
template<typename T>
struct ParamWrapper<T,
		    std::enable_if_t<std::is_same<typename T::value_type,
						  std::string>::value>> {
	std::string value;

	ParamWrapper(const T &list) noexcept
		:value(EncodeArray(list)) {}

	static constexpr bool IsBinary() noexcept {
		return false;
	}

	const char *GetValue() const noexcept {
		return value.c_str();
	}

	size_t GetSize() const noexcept {
		/* ignored for text columns */
		return 0;
	}
};

template<>
struct ParamWrapper<const std::list<std::string> *> {
	std::string value;

	ParamWrapper(const std::list<std::string> *list) noexcept
		:value(list != nullptr
		       ? EncodeArray(*list)
		       : std::string()) {}

	static constexpr bool IsBinary() noexcept {
		return false;
	}

	const char *GetValue() const noexcept {
		return value.empty() ? nullptr : value.c_str();
	}

	size_t GetSize() const noexcept {
		/* ignored for text columns */
		return 0;
	}
};

template<typename... Params>
class ParamCollector;

template<>
class ParamCollector<> {
public:
	static constexpr size_t Count() noexcept {
		return 0;
	}

	template<typename O>
	size_t Fill(O) const noexcept {
		return 0;
	}
};

template<typename T>
class ParamCollector<T> {
	ParamWrapper<T> wrapper;

public:
	explicit ParamCollector(const T &t) noexcept
		:wrapper(t) {}

	static constexpr bool HasBinary() noexcept {
		return decltype(wrapper)::IsBinary();
	}

	static constexpr size_t Count() noexcept {
		return 1;
	}

	template<typename O, typename S, typename F>
	size_t Fill(O output, S size, F format) const noexcept {
		*output = wrapper.GetValue();
		*size = wrapper.GetSize();
		*format = wrapper.IsBinary();
		return 1;
	}

	template<typename O>
	size_t Fill(O output) const noexcept {
		static_assert(!decltype(wrapper)::IsBinary(),
			      "Binary values not allowed in this overload");

		*output = wrapper.GetValue();
		return 1;
	}
};

template<typename T, typename... Rest>
class ParamCollector<T, Rest...> {
	ParamCollector<T> first;
	ParamCollector<Rest...> rest;

public:
	explicit ParamCollector(const T &t, Rest... _rest) noexcept
		:first(t), rest(_rest...) {}

	static constexpr bool HasBinary() noexcept {
		return decltype(first)::HasBinary() ||
			decltype(rest)::HasBinary();
	}

	static constexpr size_t Count() noexcept {
		return decltype(first)::Count() + decltype(rest)::Count();
	}

	template<typename O, typename S, typename F>
	size_t Fill(O output, S size, F format) const noexcept {
		const size_t nf = first.Fill(output, size, format);
		std::advance(output, nf);
		std::advance(size, nf);
		std::advance(format, nf);

		const size_t nr = rest.Fill(output, size, format);
		return nf + nr;
	}

	template<typename O>
	size_t Fill(O output) const noexcept {
		const size_t nf = first.Fill(output);
		std::advance(output, nf);

		const size_t nr = rest.Fill(output);
		return nf + nr;
	}
};

template<typename... Params>
class TextParamArray {
	ParamCollector<Params...> collector;

	static_assert(!decltype(collector)::HasBinary());

public:
	static constexpr size_t count = decltype(collector)::Count();
	const char *values[count];

	explicit TextParamArray(Params... params) noexcept
		:collector(params...)
	{
		collector.Fill(values);
	}
};

template<typename... Params>
class BinaryParamArray {
	ParamCollector<Params...> collector;

public:
	static constexpr size_t count = decltype(collector)::Count();
	const char *values[count];
	int lengths[count], formats[count];

	explicit BinaryParamArray(Params... params) noexcept
		:collector(params...)
	{
		collector.Fill(values, lengths, formats);
	}
};

template<bool binary, typename... Params>
class SelectParamArray;

template<typename... Params>
class SelectParamArray<false, Params...>
	: public TextParamArray<Params...>
{
public:
	using TextParamArray<Params...>::TextParamArray;

	constexpr const int *GetLengths() const noexcept {
		return nullptr;
	}

	constexpr const int *GetFormats() const noexcept {
		return nullptr;
	}
};

template<typename... Params>
class SelectParamArray<true, Params...>
	: public BinaryParamArray<Params...>
{
public:
	using BinaryParamArray<Params...>::BinaryParamArray;

	constexpr const int *GetLengths() const noexcept {
		return BinaryParamArray<Params...>::lengths;
	}

	constexpr const int *GetFormats() const noexcept {
		return BinaryParamArray<Params...>::formats;
	}
};

template<typename... Params>
using AutoParamArray = SelectParamArray<ParamCollector<Params...>::HasBinary(),
					Params...>;

} /* namespace Pg */
