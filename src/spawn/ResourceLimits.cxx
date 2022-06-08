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

#include "ResourceLimits.hxx"
#include "system/Error.hxx"
#include "util/djbhash.h"
#include "util/CharUtil.hxx"
#include "util/Sanitizer.hxx"

#include <assert.h>
#include <stdio.h>

/**
 * glibc has a "__rlimit_resource_t" typedef which maps to "int" in
 * C++ (probably to work around "invalid conversion from 'int' to
 * '__rlimit_resource'"); however prlimit() does not uses that
 * typedef, but uses "enum __rlimit_resource" directory.  Thus we need
 * to cast to that (internal) type.  Fortunately C++'s decltype()
 * helps us to make that internal type access portable.
 */
using my_rlimit_resource_t = decltype(RLIMIT_NLIMITS);

inline void
ResourceLimit::Get(int pid, int resource)
{
	if (prlimit(pid, my_rlimit_resource_t(resource), nullptr, this) < 0)
		throw FormatErrno("getrlimit(%d) failed", resource);
}

inline void
ResourceLimit::Set(int pid, int resource) const
{
	if (prlimit(pid, my_rlimit_resource_t(resource), this, nullptr) < 0)
		throw FormatErrno("setrlimit(%d, %lu, %lu) failed",
				  resource, (unsigned long)rlim_cur,
				  (unsigned long)rlim_max);
}

inline void
ResourceLimit::OverrideFrom(const ResourceLimit &src)
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
ResourceLimits::IsEmpty() const
{
	for (const auto &i : values)
		if (!i.IsEmpty())
			return false;

	return true;
}

[[gnu::pure]]
inline unsigned
ResourceLimits::GetHash() const
{
	return djb_hash(this, sizeof(*this));
}

char *
ResourceLimits::MakeId(char *p) const
{
	if (IsEmpty())
		return p;

	*p++ = ';';
	*p++ = 'r';
	p += sprintf(p, "%08x", GetHash());
	return p;
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
	for (unsigned i = 0; i < RLIM_NLIMITS; ++i)
		rlimit_apply(pid, i, values[i]);
}

bool
ResourceLimits::Parse(const char *s)
{
	enum {
		BOTH,
		SOFT,
		HARD,
	} which = BOTH;

	char ch;
	while ((ch = *s++) != 0) {
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

		assert(resource < RLIM_NLIMITS);
		struct rlimit *const t = &values[resource];

		unsigned long value;

		if (*s == '!') {
			value = (unsigned long)RLIM_INFINITY;
			++s;
		} else {
			char *endptr;
			value = strtoul(s, &endptr, 10);
			if (endptr == s)
				return false;

			s = endptr;

			switch (*s) {
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
				++s;
			}
		}

		switch (which) {
		case BOTH:
			t->rlim_cur = t->rlim_max = value;
			break;

		case SOFT:
			t->rlim_cur = value;
			break;

		case HARD:
			t->rlim_max = value;
			break;
		}
	}

	return true;
}
