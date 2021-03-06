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

#include "List.hxx"
#include "util/StringView.hxx"

static StringView
http_trim(StringView s) noexcept
{
	/* trim whitespace */

	s.Strip();

	/* remove quotes from quoted-string */

	if (s.size >= 2 && s.front() == '"' && s.back() == '"') {
		s.pop_front();
		s.pop_back();
	}

	/* return */

	return s;
}

static bool
http_equals(StringView a, StringView b) noexcept
{
	return http_trim(a).Equals(http_trim(b));
}

bool
http_list_contains(StringView list, const StringView item) noexcept
{
	while (!list.empty()) {
		/* XXX what if the comma is within an quoted-string? */
		auto c = list.Split(',');
		if (http_equals(c.first, item))
			return true;

		list = c.second;
	}

	return false;
}

static bool
http_equals_i(StringView a, StringView b) noexcept
{
	return http_trim(a).EqualsIgnoreCase(b);
}

bool
http_list_contains_i(StringView list, const StringView item) noexcept
{
	while (!list.empty()) {
		/* XXX what if the comma is within an quoted-string? */
		auto c = list.Split(',');
		if (http_equals_i(c.first, item))
			return true;

		list = c.second;
	}

	return false;
}
