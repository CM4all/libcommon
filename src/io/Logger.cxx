/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "Logger.hxx"
#include "Iovec.hxx"
#include "util/StaticArray.hxx"
#include "util/Exception.hxx"

#include <sys/uio.h>
#include <unistd.h>
#include <stdarg.h>

unsigned LoggerDetail::max_level = 1;

LoggerDetail::ParamWrapper<std::exception_ptr>::ParamWrapper(std::exception_ptr ep) noexcept
	:ParamWrapper<std::string>(GetFullMessage(ep)) {}

static constexpr struct iovec
MakeIovec(StringView s) noexcept
{
	return MakeIovec(s.ToVoid());
}

void
LoggerDetail::WriteV(StringView domain,
		     ConstBuffer<StringView> buffers) noexcept
{
	StaticArray<struct iovec, 64> v;

	if (!domain.empty()) {
		v.push_back(MakeIovec("["));
		v.push_back(MakeIovec(domain));
		v.push_back(MakeIovec("] "));
	}

	for (const auto i : buffers) {
		if (v.size() >= v.capacity() - 1)
			break;

		v.push_back(MakeIovec(i));
	}

	v.push_back(MakeIovec("\n"));

	ssize_t nbytes =
		writev(STDERR_FILENO, v.data(), v.size());
	(void)nbytes;
}

void
LoggerDetail::Format(unsigned level, StringView domain,
		     const char *fmt, ...) noexcept
{
	if (!CheckLevel(level))
		return;

	char buffer[2048];

	va_list ap;
	va_start(ap, fmt);
	StringView s(buffer, vsnprintf(buffer, sizeof(buffer), fmt, ap));
	va_end(ap);

	WriteV(domain, {&s, 1});
}

std::string
ChildLoggerDomain::Make(StringView parent, const char *name) noexcept
{
	if (parent.empty())
		return name;

	return std::string(parent.data, parent.size) + '/' + name;
}
