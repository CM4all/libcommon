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

#pragma once

#include <cassert>
#include <cstddef>
#include <string_view>

class MatchInfo {
	friend class RegexPointer;

	static constexpr std::size_t OVECTOR_SIZE = 30;

	const char *s;
	int n;
	int ovector[OVECTOR_SIZE];

	explicit MatchInfo(const char *_s):s(_s) {}

public:
	MatchInfo() = default;

	static constexpr std::size_t npos = std::size_t(-1);

	constexpr operator bool() const noexcept {
		return n >= 0;
	}

	constexpr std::size_t size() const noexcept {
		assert(n >= 0);

		return static_cast<std::size_t>(n);
	}

	[[gnu::pure]]
	std::string_view GetCapture(unsigned i) const noexcept {
		assert(n >= 0);

		if (i >= unsigned(n))
			return {};

		int start = ovector[2 * i];
		if (start < 0)
			return {};

		int end = ovector[2 * i + 1];
		assert(end >= start);

		return { s + start, std::size_t(end - start) };
	}

	[[gnu::pure]]
	std::size_t GetCaptureStart(unsigned i) const noexcept {

		assert(n >= 0);

		if (i >= unsigned(n))
			return npos;

		int start = ovector[2 * i];
		if (start < 0)
			return npos;

		return std::size_t(start);
	}

	[[gnu::pure]]
	std::size_t GetCaptureEnd(unsigned i) const noexcept {

		assert(n >= 0);

		if (i >= unsigned(n))
			return npos;

		int end = ovector[2 * i + 1];
		if (end < 0)
			return npos;

		return std::size_t(end);
	}
};
