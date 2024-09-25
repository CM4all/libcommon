// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Logger.hxx"
#include "Iovec.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "util/SpanCast.hxx"
#include "util/StaticVector.hxx"
#include "util/Exception.hxx"

#include <sys/uio.h>
#include <unistd.h>
#include <stdarg.h>

unsigned LoggerDetail::max_level = 1;

LoggerDetail::ParamWrapper<std::exception_ptr>::ParamWrapper(std::exception_ptr ep) noexcept
	:ParamWrapper<std::string>(GetFullMessage(std::move(ep))) {}

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
LoggerDetail::Fmt(unsigned level, std::string_view domain,
		  fmt::string_view format_str, fmt::format_args args) noexcept
{
	if (!CheckLevel(level))
		return;

	const auto msg = VFmtBuffer<1024>(format_str, args);

	std::string_view s[]{msg.c_str()};
	WriteV(domain, s);
}

std::string
ChildLoggerDomain::Make(std::string_view parent, const char *name) noexcept
{
	if (parent.empty())
		return name;

	return std::string{parent} + '/' + name;
}
