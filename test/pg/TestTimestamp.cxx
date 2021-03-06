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

#include "../../src/pg/Timestamp.hxx"

#include <gtest/gtest.h>

TEST(PgTest, ParseTimestamp)
{
	ASSERT_EQ(Pg::ParseTimestamp("1970-01-01 00:00:00+00"),
		  std::chrono::system_clock::from_time_t(0));
	ASSERT_EQ(Pg::ParseTimestamp("1970-01-01 00:00:00.05+00"),
		  std::chrono::system_clock::time_point(std::chrono::milliseconds(50)));
	ASSERT_EQ(Pg::ParseTimestamp("2009-02-13 23:31:30Z"),
		  std::chrono::system_clock::from_time_t(1234567890));

	/* with time zone */

	ASSERT_EQ(Pg::ParseTimestamp("2009-02-13 23:31:30+02"),
		  std::chrono::system_clock::from_time_t(1234567890)
		  - std::chrono::hours(2));

	ASSERT_EQ(Pg::ParseTimestamp("2009-02-13 23:31:30-01:30"),
		  std::chrono::system_clock::from_time_t(1234567890)
		  + std::chrono::hours(1)
		  + std::chrono::minutes(30));
}
