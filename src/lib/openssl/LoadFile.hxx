// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * Load OpenSSL objects from files.
 */

#ifndef SSL_LOAD_FILE_HXX
#define SSL_LOAD_FILE_HXX

#include "UniqueEVP.hxx"
#include "UniqueX509.hxx"

#include <forward_list>
#include <utility>

struct UniqueCertKey;

UniqueX509
LoadCertFile(const char *path);

std::forward_list<UniqueX509>
LoadCertChainFile(const char *path, bool first_is_ca=true);

UniqueEVP_PKEY
LoadKeyFile(const char *path);

UniqueCertKey
LoadCertKeyFile(const char *cert_path, const char *key_path);

std::pair<std::forward_list<UniqueX509>, UniqueEVP_PKEY>
LoadCertChainKeyFile(const char *cert_path, const char *key_path);

#endif
