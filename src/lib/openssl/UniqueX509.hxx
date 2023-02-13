// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
