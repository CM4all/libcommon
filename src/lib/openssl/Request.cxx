// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Request.hxx"
#include "Error.hxx"

#include <openssl/err.h>

UniqueX509_REQ
DecodeDerCertificateRequest(std::span<const std::byte> der)
{
	ERR_clear_error();

	auto data = (const unsigned char *)der.data();
	UniqueX509_REQ req(d2i_X509_REQ(nullptr, &data, der.size()));
	if (!req)
		throw SslError("d2i_X509() failed");

	return req;

}
