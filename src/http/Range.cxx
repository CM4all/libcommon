// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Range.hxx"
#include "util/NumberParser.hxx"
#include "util/StringCompare.hxx"

#include <cassert>
#include <string_view>

using std::string_view_literals::operator""sv;

/**
 * Parse a decimal integer at the beginning of the given string and
 * skip it.  Unlike strtoull(), this accepts neither a sign nor
 * leading whitespace.
 *
 * @param overflow set to true if the value does not fit into
 * uint_least64_t; #value is then left unmodified
 * @return false if the string does not begin with a digit
 */
static bool
ParseDecimal(std::string_view &s, uint_least64_t &value, bool &overflow) noexcept
{
	const auto [ptr, ec] = FromChars(s, value);
	if (ptr == s.data())
		return false;

	if (ec == std::errc::result_out_of_range)
		overflow = true;
	else if (ec != std::errc{})
		return false;

	s = s.substr(ptr - s.data());
	return true;
}

void
HttpRangeRequest::ParseRangeHeader(const char *p) noexcept
{
	assert(p != nullptr);
	assert(type == Type::NONE);
	assert(skip == 0);

	/* leaving #type as NONE means the header gets ignored and the
	   whole entity is sent; that is what RFC 9110 14.2 requires
	   for a range unit we do not understand and for a
	   ranges-specifier which is not valid.  Type::INVALID is for
	   a valid but unsatisfiable range, which RFC 9110 15.5.17
	   answers with status 416 */

	std::string_view s{p};
	if (!SkipPrefix(s, "bytes="sv))
		return;

	if (SkipPrefix(s, "-"sv)) {
		/* suffix-range */

		uint_least64_t suffix_length;
		bool overflow = false;
		if (!ParseDecimal(s, suffix_length, overflow) || !s.empty())
			return;

		if (size == 0 || (!overflow && suffix_length == 0))
			/* there is no "Content-Range" which can
			   describe an empty range */
			type = Type::INVALID;
		else {
			/* "If the selected representation is shorter
			   than the specified suffix-length, the entire
			   representation is used" (RFC 9110
			   14.1.2) */
			skip = !overflow && suffix_length < size
				? size - suffix_length
				: 0;

			type = Type::VALID;
		}

		return;
	}

	/* int-range */

	uint_least64_t first_pos;
	bool first_pos_overflow = false;
	if (!ParseDecimal(s, first_pos, first_pos_overflow) ||
	    !SkipPrefix(s, "-"sv))
		return;

	/* a missing last-pos selects the remainder of the entity
	   (this is what "wget -c" sends) */
	const bool have_last_pos = !s.empty();

	uint_least64_t last_pos;
	bool last_pos_overflow = false;
	if (have_last_pos) {
		if (!ParseDecimal(s, last_pos, last_pos_overflow) || !s.empty())
			return;

		if (!first_pos_overflow && !last_pos_overflow &&
		    last_pos < first_pos)
			/* "An int-range is invalid if the last-pos
			   value is present and less than the
			   first-pos" (RFC 9110 14.1.1) */
			return;
	}

	if (first_pos_overflow || first_pos >= size) {
		type = Type::INVALID;
		return;
	}

	/* "if the value is greater than or equal to the current
	   length of the representation data, the byte range is
	   interpreted as the remainder of the representation" (RFC
	   9110 14.1.2) */
	if (have_last_pos && !last_pos_overflow && last_pos < size)
		size = last_pos + 1;

	skip = first_pos;
	type = Type::VALID;
}
