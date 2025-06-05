// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "UniqueX509.hxx"

#include <string_view>

UniqueX509
MakeSelfIssuedDummyCert(std::string_view common_name);

UniqueX509
MakeSelfSignedDummyCert(EVP_PKEY &key, std::string_view common_name);
