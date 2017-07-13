/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "HostParser.hxx"
#include "util/CharUtil.hxx"

#include <string.h>

static inline bool
IsValidHostnameChar(char ch)
{
	return IsAlphaNumericASCII(ch) ||
		ch == '-' || ch == '.' ||
		ch == '*'; /* for wildcards */
}

static inline bool
IsValidIPv6Char(char ch)
{
	return IsDigitASCII(ch) ||
		(ch >= 'a' && ch <= 'f') ||
		(ch >= 'A' && ch <= 'F') ||
		ch == ':';
}

static const char *
FindIPv6End(const char *p)
{
	while (IsValidIPv6Char(*p))
		++p;
	return p;
}

ExtractHostResult
ExtractHost(const char *src)
{
	ExtractHostResult result{nullptr, src};
	const char *hostname;

	if (IsValidHostnameChar(*src)) {
		const char *colon = nullptr;

		hostname = src++;

		while (IsValidHostnameChar(*src) || *src == ':') {
			if (*src == ':') {
				if (colon != nullptr) {
					/* found a second colon: assume it's an IPv6
					   address */
					result.end = FindIPv6End(src + 1);
					result.host = {hostname, result.end};
					return result;
				} else
					/* remember the position of the first colon */
					colon = src;
			}

			++src;
		}

		if (colon != nullptr)
			/* hostname ends at colon */
			src = colon;

		result.end = src;
		result.host = {hostname, result.end};
	} else if (src[0] == ':' && src[1] == ':') {
		/* IPv6 address beginning with "::" */
		result.end = FindIPv6End(src + 2);
		result.host = {src, result.end};
	} else if (src[0] == '[') {
		/* "[hostname]:port" (IPv6?) */

		hostname = ++src;
		const char *end = strchr(hostname, ']');
		if (end == nullptr || end == hostname)
			/* failed, return nullptr */
			return result;

		result.host = {hostname, end};
		result.end = end + 1;
	} else {
		/* failed, return nullptr */
	}

	return result;
}
