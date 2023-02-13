// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Logger.hxx"
#include "Iovec.hxx"
#include "util/SpanCast.hxx"
#include "util/StaticVector.hxx"
#include "util/Exception.hxx"

#include <sys/uio.h>
#include <unistd.h>
#include <stdarg.h>

unsigned LoggerDetail::max_level = 1;

LoggerDetail::ParamWrapper<std::exception_ptr>::ParamWrapper(std::exception_ptr ep) noexcept
	:ParamWrapper<std::string>(GetFullMessage(ep)) {}

static struct iovec
MakeIovec(std::string_view s) noexcept
{
	return MakeIovec(AsBytes(s));
}

void
LoggerDetail::WriteV(std::string_view domain,
		     std::span<const std::string_view> buffers) noexcept
{
	StaticVector<struct iovec, 64> v;

	if (!domain.empty()) {
		v.push_back(MakeIovec("["));
		v.push_back(MakeIovec(domain));
		v.push_back(MakeIovec("] "));
	}

	for (const auto i : buffers) {
		if (v.size() >= v.max_size() - 1)
			break;

		v.push_back(MakeIovec(i));
	}

	v.push_back(MakeIovec("\n"));

	ssize_t nbytes =
		writev(STDERR_FILENO, v.data(), v.size());
	(void)nbytes;
}

void
LoggerDetail::Format(unsigned level, std::string_view domain,
		     const char *fmt, ...) noexcept
{
	if (!CheckLevel(level))
		return;

	char buffer[2048];

	va_list ap;
	va_start(ap, fmt);
	std::string_view s(buffer, vsnprintf(buffer, sizeof(buffer), fmt, ap));
	va_end(ap);

	WriteV(domain, {&s, 1});
}

std::string
ChildLoggerDomain::Make(std::string_view parent, const char *name) noexcept
{
	if (parent.empty())
		return name;

	return std::string{parent} + '/' + name;
}
