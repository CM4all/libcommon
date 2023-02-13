// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef BENG_PROXY_URANDOM_HXX
#define BENG_PROXY_URANDOM_HXX

#include <cstddef>

/**
 * Generate some pseudo-random data, and block until at least one byte
 * has been generated.  Throws if no data at all could be obtained.
 *
 * @return the number of bytes filled with random data
 */
std::size_t
UrandomRead(void *p, std::size_t size);

/**
 * Fill the given buffer with pseudo-random data.  May block.  Throws on error.
 */
void
UrandomFill(void *p, std::size_t size);

#endif
