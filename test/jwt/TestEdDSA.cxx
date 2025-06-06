// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "jwt/EdDSA.hxx"
#include "util/AllocatedArray.hxx"
#include "util/AllocatedString.hxx"
#include "util/HexFormat.hxx"
#include "util/SpanCast.hxx"
#include "util/StringBuffer.hxx"

#include <gtest/gtest.h>

#include <sodium/utils.h>

#include <stdexcept>

using std::string_view_literals::operator""sv;

static CryptoSignPublicKey
ParseBase64Key(const std::string_view base64)
{
	CryptoSignPublicKey key;

	size_t length;
	if (sodium_base642bin((unsigned char *)key.data(), key.size(),
			      base64.data(), base64.size(),
			      nullptr, &length,
			      nullptr, sodium_base64_VARIANT_URLSAFE_NO_PADDING) != 0)
		throw std::runtime_error("sodium_base642bin() failed");

	if (length != key.size())
		throw std::runtime_error("Wrong key length");

	return key;
}

static CryptoSignSecretKey
ParseBase64Key(const std::string_view d_base64,
	       const std::string_view x_base64)
{
	CryptoSignSecretKey key;

	size_t d_length;
	if (sodium_base642bin((unsigned char *)key.data(), key.size(),
			      d_base64.data(), d_base64.size(),
			      nullptr, &d_length,
			      nullptr, sodium_base64_VARIANT_URLSAFE_NO_PADDING) != 0)
		throw std::runtime_error("sodium_base642bin() failed");

	size_t x_length;
	if (sodium_base642bin((unsigned char *)key.data() + d_length,
			      key.size() - d_length,
			      x_base64.data(), x_base64.size(),
			      nullptr, &x_length,
			      nullptr, sodium_base64_VARIANT_URLSAFE_NO_PADDING) != 0)
		throw std::runtime_error("sodium_base642bin() failed");

	if (d_length + x_length != key.size())
		throw std::runtime_error("Wrong key length");

	return key;
}

TEST(JWTEdDSA, Basic)
{
	/* data from RFC 8037 A.4 */

	static constexpr auto d_base64 = "nWGxne_9WmC6hEr0kuwsxERJxWl7MmkZcDusAxyuf2A"sv;
	static constexpr auto x_base64 = "11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"sv;

	const auto key = ParseBase64Key(d_base64, x_base64);

	const auto signature = JWT::SignEdDSA(key, "eyJhbGciOiJFZERTQSJ9"sv,
					      "RXhhbXBsZSBvZiBFZDI1NTE5IHNpZ25pbmc"sv);

	ASSERT_STREQ(signature.c_str(),
		     "hgyY0il_MGCjP0JzlnLWG1PPOt7-09PGcvMg3AIbQR6dWbhijcNR4ki4iylGjg5BhVsPt9g7sVvpAr_MuM0KAg");

	const auto public_key = ParseBase64Key(x_base64);
	ASSERT_TRUE(JWT::VerifyEdDSA(public_key,
				     "eyJhbGciOiJFZERTQSJ9.RXhhbXBsZSBvZiBFZDI1NTE5IHNpZ25pbmc.hgyY0il_MGCjP0JzlnLWG1PPOt7-09PGcvMg3AIbQR6dWbhijcNR4ki4iylGjg5BhVsPt9g7sVvpAr_MuM0KAg"sv));

	const auto d = JWT::VerifyDecodeEdDSA(public_key,
					      "eyJhbGciOiJFZERTQSJ9.RXhhbXBsZSBvZiBFZDI1NTE5IHNpZ25pbmc.hgyY0il_MGCjP0JzlnLWG1PPOt7-09PGcvMg3AIbQR6dWbhijcNR4ki4iylGjg5BhVsPt9g7sVvpAr_MuM0KAg"sv);
	ASSERT_EQ(ToStringView(d), "Example of Ed25519 signing"sv);
}
