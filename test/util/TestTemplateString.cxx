/*
 * Copyright 2015-2020 Max Kellermann <max.kellermann@gmail.com>
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

#include "util/TemplateString.hxx"

#include <gtest/gtest.h>

TEST(TemplateString, CharAsString)
{
	using namespace TemplateString;
	using T = CharAsString<'?'>;
	static_assert(T::size == 1);
	ASSERT_STREQ(T::value, "?");
}

TEST(TemplateString, Concat)
{
	using namespace TemplateString;
	using T = Concat<CharAsString<'f'>, CharAsString<'o'>, CharAsString<'o'>>;
	static_assert(T::size == 3);
	ASSERT_STREQ(T::value, "foo");
}

TEST(TemplateString, InsertBefore)
{
	using namespace TemplateString;
	using T = Concat<CharAsString<'o'>, CharAsString<'o'>>;
	using V = InsertBefore<'f', T>;
	static_assert(V::size == 3);
	ASSERT_STREQ(V::value, "foo");
}
