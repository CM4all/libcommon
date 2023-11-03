// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Buffer.hxx"
#include "Error.hxx"

#include <cstddef>
#include <new> // for std::bad_alloc

SslBuffer::SslBuffer(const X509 &cert)
{
	unsigned char *data = nullptr;
	int result = i2d_X509(&cert, &data);
	if (result < 0)
		throw SslError("Failed to encode certificate");

	span = {data, static_cast<std::size_t>(result)};
}

SslBuffer::SslBuffer(const X509_NAME &name)
{
	unsigned char *data = nullptr;
	int result = i2d_X509_NAME(&name, &data);
	if (result < 0)
		throw SslError("Failed to encode name");

	span = {data, static_cast<std::size_t>(result)};
}

SslBuffer::SslBuffer(const X509_REQ &req)
{
	unsigned char *data = nullptr;
	int result = i2d_X509_REQ(&req, &data);
	if (result < 0)
		throw SslError("Failed to encode certificate request");

	span = {data, static_cast<std::size_t>(result)};
}

SslBuffer::SslBuffer(const EVP_PKEY &key)
{
	unsigned char *data = nullptr;
	int result = i2d_PrivateKey(&key, &data);
	if (result < 0)
		throw SslError("Failed to encode key");

	span = {data, static_cast<std::size_t>(result)};
}

SslBuffer::SslBuffer(const BIGNUM &bn)
{
	const std::size_t size = BN_num_bytes(&bn);
	unsigned char *data = (unsigned char *)OPENSSL_malloc(size);
	if (data == nullptr)
		throw std::bad_alloc();

	BN_bn2bin(&bn, data);

	span = {data, size};
}
