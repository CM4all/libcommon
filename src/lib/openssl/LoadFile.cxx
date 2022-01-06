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

#include "LoadFile.hxx"
#include "Key.hxx"
#include "Error.hxx"
#include "UniqueBIO.hxx"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ts.h>

UniqueX509
LoadCertFile(const char *path)
{
	ERR_clear_error();

	auto cert = TS_CONF_load_cert(path);
	if (cert == nullptr)
		throw SslError("Failed to load certificate");

	return UniqueX509(cert);
}

std::forward_list<UniqueX509>
LoadCertChainFile(const char *path, bool first_is_ca)
{
	ERR_clear_error();

	UniqueBIO bio(BIO_new_file(path, "r"));
	if (!bio)
		throw SslError(std::string("Failed to open ") + path);

	std::forward_list<UniqueX509> list;
	auto i = list.before_begin();

	UniqueX509 cert(PEM_read_bio_X509_AUX(bio.get(), nullptr,
					      nullptr, nullptr));
	if (!cert)
		throw SslError(std::string("Failed to read certificate from ") + path);

	if (first_is_ca && X509_check_ca(cert.get()) != 1)
		throw SslError(std::string("Not a CA certificate: ") + path);

	i = list.emplace_after(i, std::move(cert));

	while (true) {
		cert.reset(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
		if (!cert) {
			auto err = ERR_peek_last_error();
			if (ERR_GET_LIB(err) == ERR_LIB_PEM &&
			    ERR_GET_REASON(err) == PEM_R_NO_START_LINE) {
				ERR_clear_error();
				break;
			}

			throw SslError(std::string("Failed to read certificate chain from ") + path);
		}

		if (X509_check_ca(cert.get()) != 1)
			throw SslError(std::string("Not a CA certificate: ") + path);

		EVP_PKEY *key = X509_get_pubkey(cert.get());
		if (key == nullptr)
			throw SslError(std::string("CA certificate has no pubkey in ") + path);

		if (X509_verify(i->get(), key) <= 0)
			throw SslError(std::string("CA chain mismatch in ") + path);

		i = list.emplace_after(i, std::move(cert));
	}

	return list;
}

UniqueEVP_PKEY
LoadKeyFile(const char *path)
{
	ERR_clear_error();

	auto key = TS_CONF_load_key(path, nullptr);
	if (key == nullptr)
		throw SslError("Failed to load key");

	return UniqueEVP_PKEY(key);
}

std::pair<UniqueX509, UniqueEVP_PKEY>
LoadCertKeyFile(const char *cert_path, const char *key_path)
{
	std::pair pair{LoadCertFile(cert_path), LoadKeyFile(key_path)};
	if (!MatchModulus(*pair.first, *pair.second))
		throw std::runtime_error("Key does not match certificate");

	return pair;
}

std::pair<std::forward_list<UniqueX509>, UniqueEVP_PKEY>
LoadCertChainKeyFile(const char *cert_path, const char *key_path)
{
	std::pair pair{LoadCertChainFile(cert_path, false), LoadKeyFile(key_path)};
	if (!MatchModulus(*pair.first.front(), *pair.second))
		throw std::runtime_error("Key does not match certificate");

	return pair;
}
