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

#include "Base64.hxx"
#include "util/AllocatedArray.hxx"
#include "util/AllocatedString.hxx"
#include "util/ConstBuffer.hxx"

[[gnu::pure]]
static AllocatedString
SodiumBase64(ConstBuffer<void> src, int variant) noexcept
{
	size_t size = sodium_base64_ENCODED_LEN(src.size, variant);
	auto buffer = new char[size];
	sodium_bin2base64(buffer, size,
			  (const unsigned char *)src.data, src.size,
			  variant);
	return AllocatedString::Donate(buffer);
}

AllocatedString
UrlSafeBase64(ConstBuffer<void> src) noexcept
{
	return SodiumBase64(src, sodium_base64_VARIANT_URLSAFE_NO_PADDING);
}

AllocatedString
UrlSafeBase64(std::string_view src) noexcept
{
	return UrlSafeBase64(ConstBuffer<void>{src.data(), src.size()});
}

[[gnu::pure]]
static AllocatedArray<std::byte>
SodiumDecodeBase64(std::string_view src, int variant) noexcept
{
	AllocatedArray<std::byte> buffer(src.size());

	size_t decoded_size;
	if (sodium_base642bin((unsigned char *)buffer.data(),
			      buffer.capacity(),
			      src.data(), src.size(),
			      nullptr, &decoded_size,
			      nullptr,
			      variant) != 0)
		return nullptr;

	buffer.SetSize(decoded_size);
	return buffer;
}

AllocatedArray<std::byte>
DecodeUrlSafeBase64(std::string_view src) noexcept
{
	return SodiumDecodeBase64(src,
				  sodium_base64_VARIANT_URLSAFE_NO_PADDING);
}
