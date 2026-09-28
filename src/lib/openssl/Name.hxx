// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <openssl/ossl_typ.h>

class AllocatedString;

AllocatedString
ToString(const X509_NAME *name);

[[gnu::pure]]
AllocatedString
NidToString(const X509_NAME &name, int nid) noexcept;

[[gnu::pure]]
AllocatedString
GetCommonName(const X509 &cert) noexcept;

[[gnu::pure]]
AllocatedString
GetIssuerCommonName(const X509 &cert) noexcept;
