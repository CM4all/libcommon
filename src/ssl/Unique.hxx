/*
 * Copyright 2007-2017 Content Management AG
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

#ifndef SSL_UNIQUE_HXX
#define SSL_UNIQUE_HXX

#include <openssl/ssl.h>
#include <openssl/bn.h>

#include <memory>

struct OpenSslDelete {
	void operator()(SSL *ssl) {
		SSL_free(ssl);
	}

	void operator()(X509 *x509) {
		X509_free(x509);
	}

	void operator()(X509_REQ *r) {
		X509_REQ_free(r);
	}

	void operator()(X509_NAME *name) {
		X509_NAME_free(name);
	}

	void operator()(X509_EXTENSION *ext) {
		X509_EXTENSION_free(ext);
	}

	void operator()(X509_EXTENSIONS *sk) {
		sk_X509_EXTENSION_pop_free(sk, X509_EXTENSION_free);
	}

	void operator()(RSA *rsa) {
		RSA_free(rsa);
	}

	void operator()(EC_KEY *key) {
		EC_KEY_free(key);
	}

	void operator()(EVP_PKEY *key) {
		EVP_PKEY_free(key);
	}

	void operator()(EVP_PKEY_CTX *key) {
		EVP_PKEY_CTX_free(key);
	}

	void operator()(BIO *bio) {
		BIO_free(bio);
	}

	void operator()(BIGNUM *bn) {
		BN_free(bn);
	}
};

using UniqueSSL = std::unique_ptr<SSL, OpenSslDelete>;
using UniqueX509 = std::unique_ptr<X509, OpenSslDelete>;
using UniqueX509_REQ = std::unique_ptr<X509_REQ, OpenSslDelete>;
using UniqueX509_NAME = std::unique_ptr<X509_NAME, OpenSslDelete>;
using UniqueX509_EXTENSION = std::unique_ptr<X509_EXTENSION, OpenSslDelete>;
using UniqueX509_EXTENSIONS = std::unique_ptr<X509_EXTENSIONS, OpenSslDelete>;
using UniqueRSA = std::unique_ptr<RSA, OpenSslDelete>;
using UniqueEC_KEY = std::unique_ptr<EC_KEY, OpenSslDelete>;
using UniqueEVP_PKEY = std::unique_ptr<EVP_PKEY, OpenSslDelete>;
using UniqueEVP_PKEY_CTX = std::unique_ptr<EVP_PKEY_CTX, OpenSslDelete>;
using UniqueBIO = std::unique_ptr<BIO, OpenSslDelete>;
using UniqueBIGNUM = std::unique_ptr<BIGNUM, OpenSslDelete>;

#endif
