// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Edit.hxx"
#include "UniqueX509.hxx"
#include "GeneralName.hxx"
#include "Error.hxx"

#include "openssl/x509v3.h"

static UniqueX509_EXTENSION
MakeExt(int nid, const char *value)
{
	UniqueX509_EXTENSION ext(X509V3_EXT_conf_nid(nullptr, nullptr, nid,
						     const_cast<char *>(value)));
	if (ext == nullptr)
		throw SslError("X509V3_EXT_conf_nid() failed");

	return ext;
}

void
AddExt(X509 &cert, int nid, const char *value)
{
	X509_add_ext(&cert, MakeExt(nid, value).get(), -1);
}

void
AddAltNames(X509_REQ &req, OpenSSL::GeneralNames gn)
{
	UniqueX509_EXTENSIONS sk(sk_X509_EXTENSION_new_null());
	sk_X509_EXTENSION_push(sk.get(),
			       X509V3_EXT_i2d(NID_subject_alt_name, 0, gn.get()));

	X509_REQ_add_extensions(&req, sk.get());
}
