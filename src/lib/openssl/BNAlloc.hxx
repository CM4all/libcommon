// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <openssl/ossl_typ.h>

#include <cstddef>

template<typename T> class AllocatedArray;

[[gnu::pure]]
AllocatedArray<std::byte>
BN_bn2bin(const BIGNUM &bn) noexcept;

/**
 * Throws on error.
 */
AllocatedArray<std::byte>
BN_bn2binpad(const BIGNUM &bn, std::size_t size);
