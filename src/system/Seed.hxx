// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Urandom.hxx"

#include <random>

/**
 * Construct a std::seed_seq for the specified random number engine
 * from /dev/urandom (or getrandom()).
 *
 * Throws if /dev/urandom or getrandom() fails.
 */
template<typename Engine>
std::seed_seq
GenerateSeedSeq()
{
	uint32_t buffer[Engine::state_size * Engine::word_size / 32];
	std::size_t nbytes = UrandomRead(buffer, sizeof(buffer));
	std::size_t n = nbytes / sizeof(buffer[0]);
	return std::seed_seq(buffer, buffer + n);
}

/**
 * Construct a random number engine which is initially seeded from
 * /dev/urandom or getrandom().
 */
template<typename Engine>
Engine
MakeSeeded()
{
	/* this needs to be a local variable, because the engine
	   constructor wants a lvalue reference, so a rvalue doesn't
	   work */
	auto seed_seq = GenerateSeedSeq<Engine>();
	return Engine{seed_seq};
}
