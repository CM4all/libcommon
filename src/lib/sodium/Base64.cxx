// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Base64.hxx"
#include "util/AllocatedArray.hxx"
#include "util/AllocatedString.hxx"

AllocatedString
SodiumBase64(std::span<const std::byte> src, int variant) noexcept
{
	size_t size = sodium_base64_ENCODED_LEN(src.size(), variant);
	auto buffer = new char[size];
	sodium_bin2base64(buffer, size,
			  (const unsigned char *)src.data(), src.size(),
			  variant);
	return AllocatedString::Donate(buffer);
}

AllocatedString
SodiumBase64(std::string_view src, int variant) noexcept
{
	const std::span<const char> span{src};
	return SodiumBase64(std::as_bytes(span), variant);
}

AllocatedString
UrlSafeBase64(std::span<const std::byte> src) noexcept
{
	return SodiumBase64(src, sodium_base64_VARIANT_URLSAFE_NO_PADDING);
}

AllocatedString
UrlSafeBase64(std::string_view src) noexcept
{
	const std::span<const char> span{src};
	return UrlSafeBase64(std::as_bytes(span));
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
DecodeBase64(std::string_view src) noexcept
{
	return SodiumDecodeBase64(src, sodium_base64_VARIANT_ORIGINAL);
}

AllocatedArray<std::byte>
DecodeUrlSafeBase64(std::string_view src) noexcept
{
	return SodiumDecodeBase64(src,
				  sodium_base64_VARIANT_URLSAFE_NO_PADDING);
}
