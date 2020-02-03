/*
 * Copyright 2007-2019 Content Management AG
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

#include "OneLine.hxx"
#include "Datagram.hxx"
#include "net/Anonymize.hxx"
#include "io/FileDescriptor.hxx"
#include "util/StringBuffer.hxx"
#include "util/StringBuilder.hxx"

#include <stdio.h>
#include <time.h>

static void
AppendTimestamp(StringBuilder &b, Net::Log::TimePoint value)
{
	using namespace std::chrono;
	time_t t = duration_cast<seconds>(value.time_since_epoch()).count();
	struct tm tm;
	size_t n = strftime(b.GetTail(), b.GetRemainingSize(),
			    "%d/%b/%Y:%H:%M:%S %z", localtime_r(&t, &tm));
	if (n == 0)
		throw StringBuilder::Overflow();

	b.Extend(n);
}

gcc_const
static const char *
OptionalString(const char *p) noexcept
{
	if (p == nullptr)
		return "-";

	return p;
}

static constexpr bool
IsHarmlessChar(signed char ch) noexcept
{
	return ch >= 0x20 && ch != '"' && ch != '\\';
}

static void
AppendEscape(StringBuilder &b, StringView value)
{
	if (b.GetRemainingSize() < value.size * 4)
		throw StringBuilder::Overflow();

	char *p = b.GetTail();
	size_t n = 0;

	for (char ch : value) {
		if (IsHarmlessChar(ch))
			p[n++] = ch;
		else
			n += sprintf(p + n, "\\x%02X", (unsigned char)ch);
	}

	b.Extend(n);
}

static void
AppendAnonymize(StringBuilder &b, const char *value)
{
	auto result = AnonymizeAddress(value);
	b.Append(result.first);
	b.Append(result.second);
}

template<typename... Args>
static inline void
AppendFormat(StringBuilder &b, const char *fmt, Args&&... args)
{
	size_t size = b.GetRemainingSize();
	size_t n = snprintf(b.GetTail(), size, fmt, args...);
	if (n >= size - 1)
		throw StringBuilder::Overflow();
	b.Extend(n);
}

static char *
FormatOneLineHttp(char *buffer, size_t buffer_size,
		  const Net::Log::Datagram &d,
		  bool site, bool anonymize) noexcept
try {
	StringBuilder b(buffer, buffer_size);

	if (site) {
		b.Append(OptionalString(d.site));
		b.Append(' ');
	}

	if (d.remote_host == nullptr)
		b.Append('-');
	else if (anonymize)
		AppendAnonymize(b, d.remote_host);
	else
		b.Append(d.remote_host);

	b.Append(" - - [");

	if (d.HasTimestamp())
		AppendTimestamp(b, d.timestamp);
	else
		b.Append('-');

	const char *method = d.HasHttpMethod() &&
		http_method_is_valid(d.http_method)
		? http_method_to_string(d.http_method)
		: "?";

	AppendFormat(b, "] \"%s ", method);

	AppendEscape(b, d.http_uri);

	AppendFormat(b, " HTTP/1.1\" %u ", d.http_status);

	if (d.valid_length)
		AppendFormat(b, "%llu", (unsigned long long)d.length);
	else
		b.Append('-');

	b.Append(" \"");
	AppendEscape(b, OptionalString(d.http_referer));
	b.Append("\" \"");
	AppendEscape(b, OptionalString(d.user_agent));
	b.Append("\" ");

	if (d.valid_duration)
		AppendFormat(b, "%llu", (unsigned long long)d.duration.count());
	else
		b.Append('-');

	return b.GetTail();
} catch (StringBuilder::Overflow) {
	return buffer;
}

static char *
FormatOneLineMessage(char *buffer, size_t buffer_size,
		     const Net::Log::Datagram &d,
		     bool site) noexcept
try {
	StringBuilder b(buffer, buffer_size);

	if (site) {
		b.Append(OptionalString(d.site));
		b.Append(' ');
	}

	b.Append('[');
	if (d.HasTimestamp())
		AppendTimestamp(b, d.timestamp);
	else
		b.Append('-');

	b.Append("] ");
	AppendEscape(b, d.message);

	return b.GetTail();
} catch (StringBuilder::Overflow) {
	return buffer;
}

char *
FormatOneLine(char *buffer, size_t buffer_size,
	      const Net::Log::Datagram &d, bool site, bool anonymize) noexcept
{
	if (d.GuessIsHttpAccess())
		return FormatOneLineHttp(buffer, buffer_size, d,
					 site, anonymize);
	else if (d.message != nullptr)
		return FormatOneLineMessage(buffer, buffer_size, d, site);
	else
		return buffer;
}

bool
LogOneLine(FileDescriptor fd, const Net::Log::Datagram &d, bool site) noexcept
{
	char buffer[16384];
	char *end = FormatOneLine(buffer, sizeof(buffer) - 1, d, site);
	if (end == buffer)
		return true;

	*end++ = '\n';
	return fd.Write(buffer, end - buffer) >= 0;
}
