/*
 * Copyright 2007-2022 CM4all GmbH
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

#include "Buffer.hxx"
#include "Error.hxx"

#include <cstddef>
#include <new> // for std::bad_alloc

SslBuffer::SslBuffer(X509 &cert)
{
	unsigned char *data = nullptr;
	int result = i2d_X509(&cert, &data);
	if (result < 0)
		throw SslError("Failed to encode certificate");

	span = {data, static_cast<std::size_t>(result)};
}

SslBuffer::SslBuffer(X509_NAME &name)
{
	unsigned char *data = nullptr;
	int result = i2d_X509_NAME(&name, &data);
	if (result < 0)
		throw SslError("Failed to encode name");

	span = {data, static_cast<std::size_t>(result)};
}

SslBuffer::SslBuffer(X509_REQ &req)
{
	unsigned char *data = nullptr;
	int result = i2d_X509_REQ(&req, &data);
	if (result < 0)
		throw SslError("Failed to encode certificate request");

	span = {data, static_cast<std::size_t>(result)};
}

SslBuffer::SslBuffer(EVP_PKEY &key)
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
