// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ResourceLimits.hxx"
#include "lib/fmt/SystemError.hxx"
#include "util/Base32.hxx"
#include "util/djb_hash.hxx"
#include "util/CharUtil.hxx"
#include "util/Sanitizer.hxx"
#include "util/SpanCast.hxx"

#include <cassert>
#include <charconv>

/**
 * glibc has a "__rlimit_resource_t" typedef which maps to "int" in
 * C++ (probably to work around "invalid conversion from 'int' to
 * '__rlimit_resource'"); however prlimit() does not uses that
 * typedef, but uses "enum __rlimit_resource" directly.  Thus we need
 * to cast to that (internal) type.  Fortunately C++'s decltype()
 * helps us to make that internal type access portable.
 */
using my_rlimit_resource_t = decltype(RLIMIT_NLIMITS);

inline void
ResourceLimit::Get(int pid, int resource)
{
	if (prlimit(pid, static_cast<my_rlimit_resource_t>(resource),
		    nullptr, this) < 0)
		throw FmtErrno("getrlimit({}) failed", resource);
}

inline void
ResourceLimit::Set(int pid, int resource) const
{
	if (prlimit(pid, static_cast<my_rlimit_resource_t>(resource),
		    this, nullptr) < 0)
		throw FmtErrno("setrlimit({}, {}, {}) failed",
			       resource, rlim_cur, rlim_max);
}

inline void
ResourceLimit::OverrideFrom(const ResourceLimit &src) noexcept
{
	if (src.rlim_cur != UNDEFINED)
		rlim_cur = src.rlim_cur;

	if (src.rlim_max != UNDEFINED)
		rlim_max = src.rlim_max;
}

inline void
ResourceLimit::CompleteFrom(int pid, int resource, const ResourceLimit &src)
{
	Get(pid, resource);
	OverrideFrom(src);
}

[[gnu::pure]]
inline bool
ResourceLimits::IsEmpty() const noexcept
{
	for (const auto &i : values)
		if (!i.IsEmpty())
			return false;

	return true;
}

[[gnu::pure]]
inline std::size_t
ResourceLimits::GetHash() const noexcept
{
	return djb_hash(ReferenceAsBytes(*this));
}

char *
ResourceLimits::MakeId(char *p) const noexcept
{
	if (IsEmpty())
		return p;

	*p++ = ';';
	*p++ = 'r';
	return FormatIntBase32(p, GetHash());
}

/**
 * Replace ResourceLimit::UNDEFINED with current values.
 */
static const ResourceLimit &
complete_rlimit(int pid, int resource,
		const ResourceLimit &r, ResourceLimit &buffer)
{
	if (r.IsFull())
		/* already complete */
		return r;

	buffer.CompleteFrom(pid, resource, r);
	return buffer;
}

static void
rlimit_apply(int pid, int resource, const ResourceLimit &r)
{
	if (r.IsEmpty())
		return;

	if (HaveAddressSanitizer() && resource == RLIMIT_AS)
		/* ignore the AddressSpace limit when AddressSanitizer
		   is enabled because we'll hit this limit before we
		   can even execute the new child process */
		return;

	ResourceLimit buffer;
	const auto &r2 = complete_rlimit(pid, resource, r, buffer);
	r2.Set(pid, resource);
}

void
ResourceLimits::Apply(int pid) const
{
	for (unsigned i = 0; i < values.size(); ++i)
		rlimit_apply(pid, i, values[i]);
}

bool
ResourceLimits::Parse(std::string_view s) noexcept
{
	enum {
		BOTH,
		SOFT,
		HARD,
	} which = BOTH;

	for (auto i = s.begin(), end = s.end(); i != end;) {
		const char ch = *i++;

		unsigned resource;

		switch (ch) {
		case 'S':
			which = SOFT;
			continue;

		case 'H':
			which = HARD;
			continue;

		case 't':
			resource = RLIMIT_CPU;
			break;

		case 'f':
			resource = RLIMIT_FSIZE;
			break;

		case 'd':
			resource = RLIMIT_DATA;
			break;

		case 's':
			resource = RLIMIT_STACK;
			break;

		case 'c':
			resource = RLIMIT_CORE;
			break;

		case 'm':
			resource = RLIMIT_RSS;
			break;

		case 'u':
			resource = RLIMIT_NPROC;
			break;

		case 'n':
			resource = RLIMIT_NOFILE;
			break;

		case 'l':
			resource = RLIMIT_MEMLOCK;
			break;

		case 'v':
			resource = RLIMIT_AS;
			break;

			/* obsolete:
			   case 'x':
			   resource = RLIMIT_LOCKS;
			   break;
			*/

		case 'i':
			resource = RLIMIT_SIGPENDING;
			break;

		case 'q':
			resource = RLIMIT_MSGQUEUE;
			break;

		case 'e':
			resource = RLIMIT_NICE;
			break;

		case 'r':
			resource = RLIMIT_RTPRIO;
			break;

			/* not supported by bash's "ulimit" command
			   case ?:
			   resource = RLIMIT_RTTIME;
			   break;
			*/

		default:
			if (IsWhitespaceFast(ch))
				/* ignore whitespace */
				continue;

			return false;
		}

		assert(resource < values.size());
		struct rlimit &t = values[resource];

		unsigned long value;

		if (i == end)
			return false;

		if (*i == '!') {
			value = (unsigned long)RLIM_INFINITY;
			++i;
		} else {
			auto [ptr, ec] = std::from_chars(i, end, value, 10);
			if (ec != std::errc{})
				return false;

			i = ptr;

			switch (i != end ? *i : '\0') {
			case 'T':
				value <<= 10;
				[[fallthrough]];

			case 'G':
				value <<= 10;
				[[fallthrough]];

			case 'M':
				value <<= 10;
				[[fallthrough]];

			case 'K':
				value <<= 10;
				++i;
			}
		}

		switch (which) {
		case BOTH:
			t.rlim_cur = t.rlim_max = value;
			break;

		case SOFT:
			t.rlim_cur = value;
			break;

		case HARD:
			t.rlim_max = value;
			break;
		}
	}

	return true;
}
