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

#include "Dummy.hxx"
#include "Edit.hxx"
#include "Error.hxx"

UniqueX509
MakeSelfIssuedDummyCert(const char *common_name)
{
	UniqueX509 cert(X509_new());
	if (cert == nullptr)
		throw SslError("X509_new() failed");

	auto *name = X509_get_subject_name(cert.get());

	if (!X509_NAME_add_entry_by_NID(name, NID_commonName, MBSTRING_ASC,
					const_cast<unsigned char *>((const unsigned char *)common_name),
					-1, -1, 0))
		throw SslError("X509_NAME_add_entry_by_NID() failed");

	X509_set_issuer_name(cert.get(), name);

	X509_set_version(cert.get(), 2);
	ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1);
	X509_gmtime_adj(X509_getm_notBefore(cert.get()), 0);
	X509_gmtime_adj(X509_getm_notAfter(cert.get()), 60 * 60);

	AddExt(*cert, NID_basic_constraints, "critical,CA:TRUE");
	AddExt(*cert, NID_key_usage, "critical,keyCertSign");

	return cert;
}

UniqueX509
MakeSelfSignedDummyCert(EVP_PKEY &key, const char *common_name)
{
	auto cert = MakeSelfIssuedDummyCert(common_name);
	X509_set_pubkey(cert.get(), &key);
	if (!X509_sign(cert.get(), &key, EVP_sha256()))
		throw SslError("X509_sign() failed");

	return cert;
}
