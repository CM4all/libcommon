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

#pragma once

#include <openssl/x509.h>

#include <memory>

struct X509Deleter {
	void operator()(X509 *x509) noexcept {
		X509_free(x509);
	}

	void operator()(X509_REQ *r) noexcept {
		X509_REQ_free(r);
	}

	void operator()(X509_NAME *name) noexcept {
		X509_NAME_free(name);
	}

	void operator()(X509_EXTENSION *ext) noexcept {
		X509_EXTENSION_free(ext);
	}

	void operator()(X509_EXTENSIONS *sk) noexcept {
		sk_X509_EXTENSION_pop_free(sk, X509_EXTENSION_free);
	}
};

using UniqueX509 = std::unique_ptr<X509, X509Deleter>;
using UniqueX509_REQ = std::unique_ptr<X509_REQ, X509Deleter>;
using UniqueX509_NAME = std::unique_ptr<X509_NAME, X509Deleter>;
using UniqueX509_EXTENSION = std::unique_ptr<X509_EXTENSION, X509Deleter>;
using UniqueX509_EXTENSIONS = std::unique_ptr<X509_EXTENSIONS, X509Deleter>;

static inline auto
UpRef(X509 &cert) noexcept
{
	X509_up_ref(&cert);
	return UniqueX509(&cert);
}
