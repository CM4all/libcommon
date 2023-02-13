// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * Implementation of D. J. Bernstein's cdb hash function.
 * http://cr.yp.to/cdb/cdb.txt
 */

#ifndef DJB_HASH_H
#define DJB_HASH_H

#include "Compiler.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

gcc_pure
uint32_t
djb_hash(const void *p, size_t size);

gcc_pure
uint32_t
djb_hash_string(const char *p);

#ifdef __cplusplus
}
#endif

#endif
