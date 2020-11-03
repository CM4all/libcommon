/*
 * Copyright 2007-2020 CM4all GmbH
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

#ifndef SSL_BUFFER_HXX
#define SSL_BUFFER_HXX

#include "util/WritableBuffer.hxx"
#include "util/ConstBuffer.hxx"

#include <openssl/ssl.h>

class SslBuffer : WritableBuffer<unsigned char> {
public:
	explicit SslBuffer(X509 &cert);
	explicit SslBuffer(X509_NAME &cert);
	explicit SslBuffer(X509_REQ &req);
	explicit SslBuffer(EVP_PKEY &key);

	SslBuffer(SslBuffer &&src) noexcept
		:WritableBuffer<unsigned char>(src)
	{
		src.data = nullptr;
	}

	~SslBuffer() noexcept {
		if (data != nullptr)
			OPENSSL_free(data);
	}

	SslBuffer &operator=(SslBuffer &&src) noexcept {
		data = src.data;
		size = src.size;
		src.data = nullptr;
		return *this;
	}

	ConstBuffer<void> get() const noexcept {
		return {data, size};
	}
};

#endif
