// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "OneLine.hxx"
#include "Datagram.hxx"
#include "ContentType.hxx"
#include "http/Method.hxx"
#include "net/Anonymize.hxx"
#include "io/FileDescriptor.hxx"
#include "time/ISO8601.hxx"
#include "util/SpanCast.hxx"
#include "util/StringBuffer.hxx"
#include "util/StringBuilder.hxx"

#include <fmt/core.h>

#include <time.h>

using std::string_view_literals::operator""sv;

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

[[gnu::const]]
static std::string_view
OptionalString(std::string_view p) noexcept
{
	if (p.data() == nullptr)
		return "-"sv;

	return p;
}

static constexpr bool
IsHarmlessChar(signed char ch) noexcept
{
	return ch >= 0x20 && ch != '"' && ch != '\\';
}

static void
AppendTruncationMarker(StringBuilder &b)
{
	b.Append("..."sv);
}

static void
AppendEscape(StringBuilder &b, std::string_view value)
{
	if (b.GetRemainingSize() <= value.size() * 4)
		throw StringBuilder::Overflow();

	char *const p0 = b.GetTail(), *p = p0;

	for (char ch : value) {
		if (IsHarmlessChar(ch))
			*p++ = ch;
		else
			p = fmt::format_to(p, "\\x{:02X}"sv, static_cast<unsigned char>(ch));
	}

	b.Extend(std::distance(p0, p));
}

static void
AppendQuoted(StringBuilder &b, std::string_view value, bool truncated=false)
{
	b.Append('"');
	AppendEscape(b, value);
	if (truncated) [[unlikely]]
		AppendTruncationMarker(b);
	b.Append('"');
}

static void
AppendOptionalQuoted(StringBuilder &b, std::string_view value, bool truncated=false)
{
	if (value.data() != nullptr)
		AppendQuoted(b, value, truncated);
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

static constexpr void
VAppendFmt(StringBuilder &b,
	   fmt::string_view format_str, fmt::format_args args) noexcept
{
	const auto w = b.Write();
	auto [p, _] = fmt::vformat_to_n(w.data(), w.size(),
					format_str, args);
	b.Extend(std::distance(w.data(), p));
}

template<typename S, typename... Args>
static constexpr void
AppendFmt(StringBuilder &b,
	  const S &format_str, Args&&... args) noexcept
{
	VAppendFmt(b, format_str, fmt::make_format_args(args...));
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
		if (d.truncated_host) [[unlikely]]
			AppendTruncationMarker(b);
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

	AppendFmt(b, " \"{} "sv, method);

	AppendEscape(b, OptionalString(d.http_uri));
	if (d.truncated_http_uri) [[unlikely]]
		AppendTruncationMarker(b);

	b.Append(" HTTP/1.1\" ");

	if (d.HasHttpStatus())
		AppendFmt(b, "{}"sv, std::to_underlying(d.http_status));
	else
		b.Append('-');

	b.Append(' ');

	if (d.valid_length)
		AppendFmt(b, "{}"sv, d.length);
	else
		b.Append('-');

	if (options.show_content_type) {
		b.Append(' ');
		if (const auto content_type = ToString(d.content_type);
		    !content_type.empty())
			AppendQuoted(b, content_type);
		else
			b.Append('-');
	}

	if (options.show_http_referer) {
		b.Append(' ');
		AppendOptionalQuoted(b, d.http_referer, d.truncated_http_referer);
	}

	if (options.show_user_agent) {
		b.Append(' ');
		AppendOptionalQuoted(b, d.user_agent, d.truncated_user_agent);
	}

	b.Append(' ');
	if (d.valid_duration)
		AppendFmt(b, "{}", d.duration.count());
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

	if (!options.iso8601) {
		b.Append(' ');
		b.Append('[');
		if (d.HasTimestamp())
			AppendTimestamp(b, d.timestamp);
		else
			b.Append('-');
		b.Append(']');
	}

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
	const std::string_view line{buffer, end};
	return fd.Write(AsBytes(line)) >= 0;
}

} // namespace Net::Log
