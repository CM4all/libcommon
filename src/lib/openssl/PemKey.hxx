// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "UniqueEVP.hxx"

#include <string_view>

/**
 * Wrapper for PEM_read_bio_PUBKEY().
 *
 * Throws SslError on error.
 */
UniqueEVP_PKEY
DecodePemPublicKey(std::string_view pem);

/**
 * Wrapper for PEM_read_bio_PrivateKey().
 *
 * Throws SslError on error.
 */
UniqueEVP_PKEY
DecodePemPrivateKey(std::string_view pem);
