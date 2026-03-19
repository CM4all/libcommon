// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#ifndef SSL_NAME_HXX
#define SSL_NAME_HXX

#include <openssl/ossl_typ.h>

class AllocatedString;

AllocatedString
ToString(const X509_NAME *name);

AllocatedString
NidToString(const X509_NAME &name, int nid);

AllocatedString
GetCommonName(const X509 &cert);

AllocatedString
GetIssuerCommonName(const X509 &cert);

#endif
