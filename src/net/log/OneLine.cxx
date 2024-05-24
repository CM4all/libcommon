// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "OneLine.hxx"
#include "Datagram.hxx"
#include "ContentType.hxx"
#include "http/Method.hxx"
#include "net/Anonymize.hxx"
#include "io/FileDescriptor.hxx"
#include "time/ISO8601.hxx"
#include "util/StringBuffer.hxx"
#include "util/StringBuilder.hxx"

#include <stdio.h>
#include <time.h>

namespace Net::Log {

static void
AppendTimestamp(StringBuilder &b, TimePoint value)
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

[[gnu::const]]
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
AppendEscape(StringBuilder &b, std::string_view value)
{
	if (b.GetRemainingSize() <= value.size() * 4)
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
AppendQuoted(StringBuilder &b, std::string_view value)
{
	b.Append('"');
	AppendEscape(b, value);
	b.Append('"');
}

static void
AppendOptionalQuoted(StringBuilder &b, const char *value)
{
	if (value != nullptr)
		AppendQuoted(b, value);
	else
		b.Append('-');
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
		  const Datagram &d,
		  const OneLineOptions options) noexcept
try {
	StringBuilder b(buffer, buffer_size);

	if (options.iso8601) {
		if (d.HasTimestamp())
			b.Append(FormatISO8601(ToSystem(d.timestamp)).c_str());
		else
			b.Append('-');
		b.Append(' ');
	}

	if (options.show_site) {
		b.Append(OptionalString(d.site));
		b.Append(' ');
	}

	if (options.show_host) {
		AppendEscape(b, OptionalString(d.host));
		b.Append(' ');
	}

	if (d.remote_host == nullptr)
		b.Append('-');
	else if (options.anonymize)
		AppendAnonymize(b, d.remote_host);
	else
		b.Append(d.remote_host);

	if (options.show_forwarded_to) {
		b.Append(' ');
		b.Append(OptionalString(d.forwarded_to));
	}

	if (!options.iso8601) {
		b.Append(" - - [");

		if (d.HasTimestamp())
			AppendTimestamp(b, d.timestamp);
		else
			b.Append('-');

		b.Append(']');
	}

	const char *method = d.HasHttpMethod() &&
		http_method_is_valid(d.http_method)
		? http_method_to_string(d.http_method)
		: "?";

	AppendFormat(b, " \"%s ", method);

	AppendEscape(b, OptionalString(d.http_uri));

	b.Append(" HTTP/1.1\" ");

	if (d.HasHttpStatus())
		AppendFormat(b, "%u", d.http_status);
	else
		b.Append('-');

	b.Append(' ');

	if (d.valid_length)
		AppendFormat(b, "%llu", (unsigned long long)d.length);
	else
		b.Append('-');

	if (options.show_content_type) {
		if (const auto content_type = ToString(d.content_type);
		    !content_type.empty())
			AppendQuoted(b, content_type);
		else
			b.Append('-');
	}

	if (options.show_http_referer) {
		b.Append(' ');
		AppendOptionalQuoted(b, d.http_referer);
	}

	if (options.show_user_agent) {
		b.Append(' ');
		AppendOptionalQuoted(b, d.user_agent);
	}

	b.Append(' ');
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
		     const Datagram &d,
		     const OneLineOptions options) noexcept
try {
	StringBuilder b(buffer, buffer_size);

	if (options.show_site) {
		b.Append(OptionalString(d.site));
		b.Append(' ');
	}

	if (options.show_host) {
		AppendEscape(b, OptionalString(d.host));
		b.Append(' ');
	}

	b.Append('[');
	if (d.HasTimestamp())
		AppendTimestamp(b, d.timestamp);
	else
		b.Append('-');
	b.Append(']');

	if (d.message.data() != nullptr) {
		b.Append(' ');
		AppendEscape(b, d.message);
	}

	if (d.json.data() != nullptr) {
		b.Append(' ');

		// TODO escape?  reformat to one line?
		b.Append(d.json);
	}

	return b.GetTail();
} catch (StringBuilder::Overflow) {
	return buffer;
}

char *
FormatOneLine(char *buffer, size_t buffer_size,
	      const Datagram &d,
	      const OneLineOptions options) noexcept
{
	if (d.IsHttpAccess())
		return FormatOneLineHttp(buffer, buffer_size, d,
					 options);
	else if (d.message.data() != nullptr || d.json.data() != nullptr)
		return FormatOneLineMessage(buffer, buffer_size, d, options);
	else
		return buffer;
}

bool
LogOneLine(FileDescriptor fd, const Datagram &d,
	   const OneLineOptions options) noexcept
{
	char buffer[16384];
	char *end = FormatOneLine(buffer, sizeof(buffer) - 1, d, options);
	if (end == buffer)
		return true;

	*end++ = '\n';
	return fd.Write(buffer, end - buffer) >= 0;
}

} // namespace Net::Log
