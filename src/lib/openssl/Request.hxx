// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "UniqueX509.hxx"

#include <span>

/**
 * Decode an X.509 certificate request encoded with DER.  It is a
 * wrapper for d2i_X509_REQ().
 *
 * Throws SslError on error.
 */
UniqueX509_REQ
DecodeDerCertificateRequest(std::span<const std::byte> der);
