// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "LoadFile.hxx"
#include "UniqueCertKey.hxx"
#include "Key.hxx"
#include "Error.hxx"
#include "UniqueBIO.hxx"
#include "lib/fmt/RuntimeError.hxx"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ts.h>

#include <string>

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

		if (int result = X509_verify(i->get(), key); result <= 0) {
			if (result < 0)
				throw SslError("Failed to verify CA chain");
			else
				throw FmtRuntimeError("CA chain mismatch in {}",
						      path);
		}

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

UniqueCertKey
LoadCertKeyFile(const char *cert_path, const char *key_path)
{
	UniqueCertKey ck{
		LoadCertFile(cert_path),
		LoadKeyFile(key_path),
	};

	if (!MatchModulus(*ck.cert, *ck.key))
		throw std::runtime_error("Key does not match certificate");

	return ck;
}

std::pair<std::forward_list<UniqueX509>, UniqueEVP_PKEY>
LoadCertChainKeyFile(const char *cert_path, const char *key_path)
{
	std::pair pair{LoadCertChainFile(cert_path, false), LoadKeyFile(key_path)};
	if (!MatchModulus(*pair.first.front(), *pair.second))
		throw std::runtime_error("Key does not match certificate");

	return pair;
}
