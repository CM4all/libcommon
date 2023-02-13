// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <openssl/ossl_typ.h>

#include <forward_list>
#include <string>

[[gnu::pure]]
std::forward_list<std::string>
GetSubjectAltNames(X509 &cert) noexcept;
