/*
 * Copyright 2020-2021 CM4all GmbH
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

#include "util/Compiler.h"
#include "util/StringBuffer.hxx"

#include <sodium/utils.h>

#include <cstddef> // for std::byte
#include <string_view>

template<typename T> struct ConstBuffer;
class AllocatedString;
template<typename T> class AllocatedArray;

template<std::size_t src_size, int variant>
gcc_pure
auto
FixedBase64(const void *src) noexcept
{
	StringBuffer<sodium_base64_ENCODED_LEN(src_size, variant)> dest;
	sodium_bin2base64(dest.data(), dest.capacity(),
			  (const unsigned char *)src, src_size,
			  variant);
	return dest;
}

gcc_pure
AllocatedString
UrlSafeBase64(ConstBuffer<void> src) noexcept;

gcc_pure
AllocatedString
UrlSafeBase64(std::string_view src) noexcept;

/**
 * @return the decoded string or a nulled instance on error
 */
gcc_pure
AllocatedArray<std::byte>
DecodeUrlSafeBase64(std::string_view src) noexcept;
