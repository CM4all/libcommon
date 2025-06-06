// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "MatchData.hxx"

#include <pcre2.h>

#include <string_view>

class RegexPointer {
protected:
	pcre2_code_8 *re = nullptr;

	unsigned n_capture = 0;

public:
	constexpr bool IsDefined() const noexcept {
		return re != nullptr;
	}

	[[gnu::pure]]
	MatchData Match(std::string_view s) const noexcept {
		MatchData match_data{
			pcre2_match_data_create_from_pattern_8(re, nullptr),
			s.data(),
		};

		int n = pcre2_match_8(re, (PCRE2_SPTR8)s.data(), s.size(),
				      0, 0,
				      match_data.match_data, nullptr);
		if (n < 0)
			/* no match (or error) */
			return {};

		match_data.n = n;

		if (n_capture >= match_data.n)
			/* in its return value, PCRE omits mismatching
			   optional captures if (and only if) they are
			   the last capture; this kludge works around
			   this */
			match_data.n = n_capture + 1;

		return match_data;
	}
};
