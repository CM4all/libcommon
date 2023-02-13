// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "RegexPointer.hxx"

#include <utility>

class UniqueRegex : public RegexPointer {
public:
	UniqueRegex() = default;

	UniqueRegex(const char *pattern, bool anchored, bool capture) {
		Compile(pattern, anchored, capture);
	}

	UniqueRegex(UniqueRegex &&src) noexcept:RegexPointer(src) {
		src.re = nullptr;
	}

	~UniqueRegex() noexcept {
		if (re != nullptr)
			pcre2_code_free_8(re);
	}

	UniqueRegex &operator=(UniqueRegex &&src) noexcept {
		using std::swap;
		swap<RegexPointer>(*this, src);
		return *this;
	}

	/**
	 * Throws Pcre::Error on error.
	 */
	void Compile(const char *pattern, bool anchored, bool capture);
};
