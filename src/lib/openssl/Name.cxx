// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Name.hxx"
#include "MemBio.hxx"

#include <openssl/x509.h>

AllocatedString
ToString(X509_NAME *name)
{
	if (name == nullptr)
		return nullptr;

	return BioWriterToString([name](BIO &bio){
			X509_NAME_print_ex(&bio, name, 0,
					   ASN1_STRFLGS_UTF8_CONVERT | XN_FLAG_SEP_COMMA_PLUS);
		});
}

AllocatedString
NidToString(X509_NAME &name, int nid)
{
	char buffer[1024];
	int len = X509_NAME_get_text_by_NID(&name, nid, buffer, sizeof(buffer));
	if (len < 0)
		return nullptr;

	const std::string_view src(buffer, len);
	return AllocatedString(src);
}

static AllocatedString
GetCommonName(X509_NAME &name)
{
	return NidToString(name, NID_commonName);
}

AllocatedString
GetCommonName(X509 &cert)
{
	X509_NAME *subject = X509_get_subject_name(&cert);
	return subject != nullptr
		? GetCommonName(*subject)
		: nullptr;
}

AllocatedString
GetIssuerCommonName(X509 &cert)
{
	X509_NAME *subject = X509_get_issuer_name(&cert);
	return subject != nullptr
		? GetCommonName(*subject)
		: nullptr;
}
