// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Certificate.hxx"
#include "Error.hxx"

#include <openssl/err.h>

UniqueX509
DecodeDerCertificate(std::span<const std::byte> der)
{
	ERR_clear_error();

	auto data = (const unsigned char *)der.data();
	UniqueX509 cert(d2i_X509(nullptr, &data, der.size()));
	if (!cert)
		throw SslError("d2i_X509() failed");

	return cert;

}
