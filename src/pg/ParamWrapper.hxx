/*
 * Copyright 2007-2022 CM4all GmbH
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

#include <fmt/format.h>

#include <array>
#include <concepts>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace Pg {

template<typename T>
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

template<typename T>
requires std::is_integral_v<T>
struct ParamWrapper<T> {
	fmt::format_int buffer;

	constexpr ParamWrapper(T value) noexcept
		:buffer(value) {}

	const char *GetValue() const noexcept {
		return buffer.c_str();
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
struct ParamWrapper<Serial> {
	fmt::format_int buffer;

	ParamWrapper(Serial s) noexcept
		:buffer(s.get()) {}

	const char *GetValue() const noexcept {
		return buffer.c_str();
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
struct ParamWrapper<BigSerial> {
	fmt::format_int buffer;

	ParamWrapper(BigSerial s) noexcept
		:buffer(s.get()) {}

	const char *GetValue() const noexcept {
		return buffer.c_str();
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
		return (const char *)(const void *)value.data();
	}

	static constexpr bool IsBinary() noexcept {
		return true;
	}

	constexpr size_t GetSize() const noexcept {
		return value.size();
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

template<>
struct ParamWrapper<std::string> : ParamWrapper<const char *> {
	ParamWrapper(const std::string &_value) noexcept
		:ParamWrapper<const char *>(_value.c_str()) {}
};

/**
 * Specialization for STL container types of std::string instances.
 */
template<typename T>
requires std::convertible_to<typename T::value_type, std::string_view> && std::forward_iterator<typename T::const_iterator>
struct ParamWrapper<T> {
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

template<typename T>
requires std::convertible_to<typename T::value_type, std::string_view> && std::forward_iterator<typename T::const_iterator>
struct ParamWrapper<const T *> {
	std::string value;

	ParamWrapper(const T *list) noexcept
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

template<typename T>
struct ParamWrapper<std::optional<T>> {
	std::optional<ParamWrapper<T>> inner;

	ParamWrapper(const std::optional<T> &_value) noexcept
	{
		if (_value)
			inner.emplace(*_value);
	}

	static constexpr bool IsBinary() noexcept {
		return ParamWrapper<T>::IsBinary();
	}

	const char *GetValue() const noexcept {
		return inner ? inner->GetValue() : nullptr;
	}

	size_t GetSize() const noexcept {
		return IsBinary() && inner ? inner->GetSize() : 0U;
	}
};


template<typename... Params>
class ParamCollector {
	std::tuple<ParamWrapper<Params>...> wrappers;

public:
	explicit ParamCollector(const Params&... params) noexcept
		:wrappers(params...)
	{
	}

	static constexpr bool HasBinary() noexcept {
		return (ParamWrapper<Params>::IsBinary() || ...);
	}

	static constexpr size_t Count() noexcept {
		return sizeof...(Params);
	}

	template<typename O, typename S, typename F>
	size_t Fill(O output, S size, F format) const noexcept {
		return std::apply([&](const auto &...w){
			return (FillOne(w, output, size, format) + ...);
		}, wrappers);
	}

	template<typename O>
	size_t Fill(O output) const noexcept {
		return std::apply([&](const auto &...w){
			return (FillOne(w, output) + ...);
		}, wrappers);
	}

private:
	template<typename W, typename O, typename S, typename F>
	static std::size_t FillOne(const W &wrapper, O &output,
				   S &size, F &format) noexcept {
		*output++ = wrapper.GetValue();
		*size++ = wrapper.GetSize();
		*format++ = wrapper.IsBinary();
		return 1;
	}

	template<typename W, typename O>
	static std::size_t FillOne(const W &wrapper, O &output) noexcept {
		*output++ = wrapper.GetValue();
		return 1;
	}
};

class EmptyParamArray {
public:
	constexpr std::size_t size() const noexcept {
		return 0;
	}

	constexpr const char *const*GetValues() const noexcept {
		return nullptr;
	}

	constexpr const int *GetLengths() const noexcept {
		return nullptr;
	}

	constexpr const int *GetFormats() const noexcept {
		return nullptr;
	}
};

template<typename... Params>
class TextParamArray {
	ParamCollector<Params...> collector;

	static_assert(!decltype(collector)::HasBinary());

	static constexpr size_t count = decltype(collector)::Count();
	std::array<const char *, count> values;

public:
	explicit TextParamArray(const Params&... params) noexcept
		:collector(params...)
	{
		collector.Fill(values.begin());
	}

	constexpr std::size_t size() const noexcept {
		return count;
	}

	constexpr const char *const*GetValues() const noexcept {
		return values.data();
	}

	constexpr const int *GetLengths() const noexcept {
		return nullptr;
	}

	constexpr const int *GetFormats() const noexcept {
		return nullptr;
	}
};

template<typename... Params>
class BinaryParamArray {
	ParamCollector<Params...> collector;

	static constexpr size_t count = decltype(collector)::Count();
	std::array<const char *, count> values;
	std::array<int, count> lengths;
	std::array<int, count> formats;

public:
	explicit BinaryParamArray(const Params&... params) noexcept
		:collector(params...)
	{
		collector.Fill(values.begin(),
			       lengths.begin(),
			       formats.begin());
	}

	constexpr std::size_t size() const noexcept {
		return count;
	}

	constexpr const char *const*GetValues() const noexcept {
		return values.data();
	}

	constexpr const int *GetLengths() const noexcept {
		return lengths.data();
	}

	constexpr const int *GetFormats() const noexcept {
		return formats.data();
	}
};

template<typename... Params>
using AutoParamArray =
	std::conditional_t<sizeof...(Params) == 0,
			   EmptyParamArray,
			   std::conditional_t<ParamCollector<Params...>::HasBinary(),
					      BinaryParamArray<Params...>,
					      TextParamArray<Params...>>>;

template<typename T>
concept ParamArray = requires (const T &t) {
	{ t.size() } noexcept -> std::convertible_to<std::size_t>;
	{ t.GetValues() } noexcept -> std::same_as<const char *const*>;
	{ t.GetLengths() } noexcept -> std::same_as<const int *>;
	{ t.GetFormats() } noexcept -> std::same_as<const int *>;
};

} /* namespace Pg */
