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

#pragma once

#include "MatchInfo.hxx"
#include "util/StringView.hxx"
#include "util/Compiler.h"

#include <pcre.h>

#include <algorithm>

class RegexPointer {
protected:
	pcre *re = nullptr;
	pcre_extra *extra = nullptr;

	unsigned n_capture = 0;

public:
	constexpr bool IsDefined() const noexcept {
		return re != nullptr;
	}

	gcc_pure
	bool Match(StringView s) const noexcept {
		/* we don't need the data written to ovector, but PCRE can
		   omit internal allocations if we pass a buffer to
		   pcre_exec() */
		int ovector[MatchInfo::OVECTOR_SIZE];
		return pcre_exec(re, extra, s.data, s.size,
				 0, 0, ovector, MatchInfo::OVECTOR_SIZE) >= 0;
	}

	gcc_pure
	MatchInfo MatchCapture(StringView s) const noexcept {
		MatchInfo mi(s.data);
		mi.n = pcre_exec(re, extra, s.data, s.size,
				 0, 0, mi.ovector, mi.OVECTOR_SIZE);
		if (mi.n == 0)
			/* not enough room in the array - assume it's full */
			mi.n = mi.OVECTOR_SIZE / 3;
		else if (mi.n > 0 && n_capture >= unsigned(mi.n))
			/* in its return value, PCRE omits mismatching
			   optional captures if (and only if) they are
			   the last capture; this kludge works around
			   this */
			mi.n = std::min<unsigned>(n_capture + 1,
						  mi.OVECTOR_SIZE / 3);
		return mi;
	}
};

class UniqueRegex : public RegexPointer {
public:
	UniqueRegex() = default;

	UniqueRegex(const char *pattern, bool anchored, bool capture) {
		Compile(pattern, anchored, capture);
	}

	UniqueRegex(UniqueRegex &&src) noexcept:RegexPointer(src) {
		src.re = nullptr;
		src.extra = nullptr;
	}

	~UniqueRegex() noexcept {
		pcre_free(re);
#ifdef PCRE_CONFIG_JIT
		pcre_free_study(extra);
#else
		pcre_free(extra);
#endif
	}

	UniqueRegex &operator=(UniqueRegex &&src) noexcept {
		std::swap<RegexPointer>(*this, src);
		return *this;
	}

	/**
	 * Throws std::runtime_error on error.
	 */
	void Compile(const char *pattern, bool anchored, bool capture);
};
