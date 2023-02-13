// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * OpenSSL certificate utilities.
 */

#pragma once

#include "UniqueX509.hxx"

#include <span>

/**
 * Decode an X.509 certificate encoded with DER.  It is a wrapper for
 * d2i_X509().
 *
 * Throws SslError on error.
 */
UniqueX509
DecodeDerCertificate(std::span<const std::byte> der);
