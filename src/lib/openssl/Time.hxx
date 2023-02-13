// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef SSL_TIME_HXX
#define SSL_TIME_HXX

#include <openssl/ossl_typ.h>

class AllocatedString;

AllocatedString
FormatTime(const ASN1_TIME &t);

AllocatedString
FormatTime(const ASN1_TIME *t);

#endif
