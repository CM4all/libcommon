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

#include "Error.hxx"
#include "util/StringCompare.hxx"

bool
Pg::Error::IsType(const char *expected_type) const noexcept
{
	const char *actual_type = GetType();
	return actual_type != nullptr &&
		StringIsEqual(actual_type, expected_type);
}

bool
Pg::Error::HasTypePrefix(std::string_view type_prefix) const noexcept
{
	const char *actual_type = GetType();
	return actual_type != nullptr &&
		StringStartsWith(actual_type, type_prefix);
}

bool
Pg::Error::IsFatal() const noexcept
{
	/* Class 08 - Connection Exception; see
	   https://www.postgresql.org/docs/9.6/errcodes-appendix.html */
	return HasTypePrefix("08");
}

bool
Pg::Error::IsDataException() const noexcept
{
	/* Class 02 - Data Exception; see
	   https://www.postgresql.org/docs/9.6/errcodes-appendix.html */
	return HasTypePrefix("22");
}
