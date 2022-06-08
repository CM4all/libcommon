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

#ifndef LOGGER_HXX
#define LOGGER_HXX

#include "util/StringView.hxx"
#include "util/Compiler.h"

#include <cstdint>
#include <array>
#include <exception>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <stdio.h>
#include <inttypes.h>

namespace LoggerDetail {

template<typename T>
struct ParamWrapper;

template<>
struct ParamWrapper<std::string_view> {
	std::string_view value;

	constexpr explicit ParamWrapper(std::string_view _value) noexcept
		:value(_value) {}

	constexpr std::string_view GetValue() const noexcept {
		return value;
	}
};

template<>
struct ParamWrapper<StringView> : ParamWrapper<std::string_view> {
	using ParamWrapper<std::string_view>::ParamWrapper;
};

template<>
struct ParamWrapper<const char *> : ParamWrapper<std::string_view> {
	using ParamWrapper<std::string_view>::ParamWrapper;
};

template<>
struct ParamWrapper<char *> : ParamWrapper<std::string_view> {
	using ParamWrapper<std::string_view>::ParamWrapper;
};

template<>
struct ParamWrapper<std::string> {
	std::string value;

	template<typename S>
	explicit ParamWrapper(S &&_value) noexcept
		:value(std::forward<S>(_value)) {}

	[[gnu::pure]]
	std::string_view GetValue() const noexcept {
		return value;
	}
};

template<>
struct ParamWrapper<std::exception_ptr> : ParamWrapper<std::string> {
	explicit ParamWrapper(std::exception_ptr ep) noexcept;
};

template<>
struct ParamWrapper<int> {
	char data[16];
	size_t size;

	ParamWrapper(int _value) noexcept
		:size(sprintf(data, "%i", _value)) {}

	std::string_view GetValue() const noexcept {
		return {data, size};
	}
};

template<>
struct ParamWrapper<unsigned> {
	char data[16];
	size_t size;

	ParamWrapper(int _value) noexcept
		:size(sprintf(data, "%u", _value)) {}

	std::string_view GetValue() const noexcept {
		return {data, size};
	}
};

template<>
struct ParamWrapper<int64_t> {
	char data[32];
	size_t size;

	ParamWrapper(int64_t _value) noexcept
		:size(sprintf(data, "%" PRId64, _value)) {}

	std::string_view GetValue() const noexcept {
		return {data, size};
	}
};

template<>
struct ParamWrapper<uint64_t> {
	char data[32];
	size_t size;

	ParamWrapper(uint64_t _value) noexcept
		:size(sprintf(data, "%" PRIu64, _value)) {}

	std::string_view GetValue() const noexcept {
		return {data, size};
	}
};

template<typename... Params>
class ParamArray {
	std::tuple<ParamWrapper<Params>...> wrappers;

public:
	static constexpr size_t count = sizeof...(Params);
	std::array<std::string_view, count> values;

	explicit ParamArray(Params... params) noexcept
		:wrappers(params...)
	{
		std::apply([this](const auto &...w){
			auto *i = values.data();
			((*i++ = w.GetValue()), ...);
		}, wrappers);
	}
};

extern unsigned max_level;

inline bool
CheckLevel(unsigned level) noexcept
{
	return level <= max_level;
}

void
WriteV(std::string_view domain, std::span<const std::string_view> buffers) noexcept;

template<typename... Params>
void
LogConcat(unsigned level, std::string_view domain, Params... _params) noexcept
{
	if (!CheckLevel(level))
		return;

	const ParamArray<Params...> params(_params...);
	WriteV(domain, params.values);
}

gcc_printf(3, 4)
void
Format(unsigned level, std::string_view domain, const char *fmt, ...) noexcept;

} /* namespace LoggerDetail */

inline void
SetLogLevel(unsigned level) noexcept
{
	LoggerDetail::max_level = level;
}

inline bool
CheckLogLevel(unsigned level) noexcept
{
	return LoggerDetail::CheckLevel(level);
}

template<typename D, typename... Params>
void
LogConcat(unsigned level, D &&domain, Params... params) noexcept
{
	LoggerDetail::LogConcat(level, std::forward<D>(domain),
				std::forward<Params>(params)...);
}

template<typename D, typename... Params>
void
LogFormat(unsigned level, D &&domain,
	  const char *fmt, Params... params) noexcept
{
	LoggerDetail::Format(level, std::forward<D>(domain),
			     fmt, std::forward<Params>(params)...);
}

template<typename Domain>
class BasicLogger : public Domain {
public:
	BasicLogger() = default;

	template<typename D>
	explicit BasicLogger(D &&_domain)
		:Domain(std::forward<D>(_domain)) {}

	static bool CheckLevel(unsigned level) noexcept {
		return LoggerDetail::CheckLevel(level);
	}

	template<typename... Params>
	void operator()(unsigned level, Params... params) const noexcept {
		LoggerDetail::LogConcat(level, GetDomain(),
					std::forward<Params>(params)...);
	}

	template<typename... Params>
	void Format(unsigned level,
		    const char *fmt, Params... params) const noexcept {
		LoggerDetail::Format(level, GetDomain(),
				     fmt, std::forward<Params>(params)...);
	}

	std::string_view GetDomain() const noexcept {
		return Domain::GetDomain();
	}

private:
	void WriteV(std::span<const std::string_view> buffers) const noexcept {
		LoggerDetail::WriteV(GetDomain(), buffers);
	}
};

class StringLoggerDomain {
	std::string name;

public:
	StringLoggerDomain() = default;

	template<typename T>
	explicit StringLoggerDomain(T &&_name) noexcept
		:name(std::forward<T>(_name)) {}

	std::string_view GetDomain() const noexcept {
		return {name.data(), name.length()};
	}
};

class Logger : public BasicLogger<StringLoggerDomain> {
public:
	Logger() = default;

	template<typename D>
	explicit Logger(D &&_domain)
		:BasicLogger(std::forward<D>(_domain)) {}
};

/**
 * An empty logger domain, to be used by #RootLogger.
 */
class NullLoggerDomain {
public:
	constexpr std::string_view GetDomain() const noexcept {
		return {};
	}
};

/**
 * An empty logger domain, to be used by #RootLogger.
 */
class RootLogger : public BasicLogger<NullLoggerDomain> {
};

class ChildLoggerDomain : public StringLoggerDomain {
	std::string name;

public:
	template<typename P>
	ChildLoggerDomain(P &&parent, const char *_name) noexcept
		:StringLoggerDomain(Make(parent.GetDomain(), _name)) {}

private:
	static std::string Make(std::string_view parent, const char *name) noexcept;
};

class ChildLogger : public BasicLogger<ChildLoggerDomain> {
public:
	template<typename P>
	ChildLogger(P &&parent, const char *_name) noexcept
		:BasicLogger(ChildLoggerDomain(std::forward<P>(parent),
					       _name)) {}
};

/**
 * A lighter version of #StringLoggerDomain which uses a literal
 * string as its domain.
 */
class LiteralLoggerDomain {
	std::string_view domain;

public:
	constexpr explicit LiteralLoggerDomain(std::string_view _domain={}) noexcept
		:domain(_domain) {}

	constexpr std::string_view GetDomain() const noexcept {
		return domain;
	}
};

/**
 * A lighter version of #Logger which uses a literal string as its
 * domain.
 */
class LLogger : public BasicLogger<LiteralLoggerDomain> {
public:
	LLogger() = default;

	template<typename D>
	explicit LLogger(D &&_domain) noexcept
		:BasicLogger(std::forward<D>(_domain)) {}
};

class LoggerDomainFactory {
public:
	virtual std::string MakeLoggerDomain() const noexcept = 0;
};

class LazyLoggerDomain {
	LoggerDomainFactory &factory;

	mutable std::string cache;

public:
	explicit LazyLoggerDomain(LoggerDomainFactory &_factory) noexcept
		:factory(_factory) {}

	std::string_view GetDomain() const noexcept {
		if (cache.empty())
			cache = factory.MakeLoggerDomain();
		return cache;
	}
};

class LazyDomainLogger : public BasicLogger<LazyLoggerDomain> {
public:
	explicit LazyDomainLogger(LoggerDomainFactory &_factory) noexcept
		:BasicLogger(_factory) {}
};

#endif
