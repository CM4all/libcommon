/*
 * Copyright 2007-2017 Content Management AG
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
#include "util/ConstBuffer.hxx"
#include "util/Compiler.h"

#include <string>
#include <array>
#include <exception>

#include <stdio.h>

namespace LoggerDetail {

template<typename T>
struct ParamWrapper;

template<>
struct ParamWrapper<StringView> {
    StringView value;

    constexpr explicit ParamWrapper(StringView _value):value(_value) {}

    constexpr StringView GetValue() const {
        return value;
    }
};

template<>
struct ParamWrapper<const char *> : ParamWrapper<StringView> {
    explicit ParamWrapper(const char *_value)
      :ParamWrapper<StringView>(_value) {}
};

template<>
struct ParamWrapper<char *> : ParamWrapper<StringView> {
    explicit ParamWrapper(char *_value)
      :ParamWrapper<StringView>(_value) {}
};

template<>
struct ParamWrapper<std::string> {
    std::string value;

    template<typename S>
    explicit ParamWrapper(S &&_value)
        :value(std::forward<S>(_value)) {}

    gcc_pure
    StringView GetValue() const {
        return {value.data(), value.length()};
    }
};

template<>
struct ParamWrapper<std::exception_ptr> : ParamWrapper<std::string> {
    explicit ParamWrapper(std::exception_ptr ep);
};

template<>
struct ParamWrapper<int> {
    char data[16];
    size_t size;

    ParamWrapper(int _value):size(sprintf(data, "%i", _value)) {}

    StringView GetValue() const {
        return {data, size};
    }
};

template<>
struct ParamWrapper<unsigned> {
    char data[16];
    size_t size;

    ParamWrapper(int _value):size(sprintf(data, "%u", _value)) {}

    StringView GetValue() const {
        return {data, size};
    }
};

template<typename... Params>
class ParamCollector;

template<>
class ParamCollector<> {
public:
    static constexpr size_t Count() {
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
    explicit ParamCollector(const T &t):wrapper(t) {}

    static constexpr size_t Count() {
        return 1;
    }

    template<typename O>
    O Fill(O output) const noexcept {
        *output++ = wrapper.GetValue();
        return output;
    }
};

template<typename T, typename... Rest>
class ParamCollector<T, Rest...> {
    ParamCollector<T> first;
    ParamCollector<Rest...> rest;

public:
    explicit ParamCollector(const T &t, Rest... _rest)
        :first(t), rest(_rest...) {}

    static constexpr size_t Count() {
        return decltype(first)::Count() + decltype(rest)::Count();
    }

    template<typename O>
    O Fill(O output) const noexcept {
        return rest.Fill(first.Fill(output));
    }
};

template<typename... Params>
class ParamArray {
    ParamCollector<Params...> collector;

public:
    static constexpr size_t count = decltype(collector)::Count();
    std::array<StringView, decltype(collector)::Count()> values;

    explicit ParamArray(Params... params):collector(params...) {
        collector.Fill(&values.front());
    }
};

extern unsigned max_level;

inline bool
CheckLevel(unsigned level)
{
    return level < max_level;
}

void
WriteV(StringView domain, ConstBuffer<StringView> buffers) noexcept;

template<typename... Params>
void
LogConcat(unsigned level, StringView domain, Params... _params) noexcept
{
    if (!CheckLevel(level))
        return;

    const ParamArray<Params...> params(_params...);
    WriteV(domain,
           {&params.values.front(), params.values.size()});
}

gcc_printf(3, 4)
void
Format(unsigned level, StringView domain, const char *fmt, ...) noexcept;

} /* namespace LoggerDetail */

inline void
SetLogLevel(unsigned level)
{
    LoggerDetail::max_level = level;
}

inline bool
CheckLogLevel(unsigned level)
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

    static bool CheckLevel(unsigned level) {
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

private:
    StringView GetDomain() const {
        return Domain::GetDomain();
    }

    void WriteV(ConstBuffer<StringView> buffers) const noexcept {
        LoggerDetail::WriteV(GetDomain(), buffers);
    }
};

class StringLoggerDomain {
    std::string name;

public:
    StringLoggerDomain() = default;

    template<typename T>
    explicit StringLoggerDomain(T &&_name)
        :name(std::forward<T>(_name)) {}

    StringView GetDomain() const {
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
 * A lighter version of #StringLoggerDomain which uses a literal
 * string as its domain.
 */
class LiteralLoggerDomain {
    StringView domain;

public:
    constexpr explicit LiteralLoggerDomain(StringView _domain=nullptr)
        :domain(_domain) {}

    constexpr StringView GetDomain() const {
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
    explicit LLogger(D &&_domain)
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
    explicit LazyLoggerDomain(LoggerDomainFactory &_factory)
        :factory(_factory) {}

    StringView GetDomain() const {
        if (cache.empty())
            cache = factory.MakeLoggerDomain();
        return {cache.data(), cache.length()};
    }
};

class LazyDomainLogger : public BasicLogger<LazyLoggerDomain> {
public:
    explicit LazyDomainLogger(LoggerDomainFactory &_factory)
        :BasicLogger(_factory) {}
};

#endif
