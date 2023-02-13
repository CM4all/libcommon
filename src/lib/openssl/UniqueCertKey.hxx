// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "UniqueX509.hxx"
#include "UniqueEVP.hxx"

struct UniqueCertKey {
	UniqueX509 cert;
	UniqueEVP_PKEY key;

	operator bool() const noexcept {
		return (bool)key;
	}
};

static inline UniqueCertKey
UpRef(const UniqueCertKey &cert_key) noexcept
{
	return {
		UpRef(*cert_key.cert),
		UpRef(*cert_key.key),
	};
}
