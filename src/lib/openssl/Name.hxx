// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef SSL_NAME_HXX
#define SSL_NAME_HXX

#include <openssl/ossl_typ.h>

class AllocatedString;

AllocatedString
ToString(X509_NAME *name);

AllocatedString
NidToString(X509_NAME &name, int nid);

AllocatedString
GetCommonName(X509 &cert);

AllocatedString
GetIssuerCommonName(X509 &cert);

#endif
