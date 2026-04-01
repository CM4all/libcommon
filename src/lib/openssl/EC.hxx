// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "BN.hxx"
#include "UniqueEC.hxx"
#include "Error.hxx"

inline UniqueECDSA_SIG
NewUniqueECDSA_SIG()
{
	UniqueECDSA_SIG sig{ECDSA_SIG_new()};
	if (!sig)
		throw SslError{};

	return sig;
}

template<bool clear>
inline UniqueECDSA_SIG
NewUniqueECDSA_SIG(UniqueBIGNUM<clear> &&r, UniqueBIGNUM<clear> &&s)
{
	auto sig = NewUniqueECDSA_SIG();

	if (!ECDSA_SIG_set0(sig.get(), r.get(), s.get()))
		throw SslError{};

	r.release();
	s.release();

	return sig;
}
