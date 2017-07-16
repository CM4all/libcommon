/*! \file
 * \brief Memory poisoning.
 *
 * \author Max Kellermann (mk@cm4all.com)
 */

#ifndef POISON_H
#define POISON_H

#include "Compiler.h"

#ifdef POISON
#include <string.h>
#endif

/**
 * Poisons the specified memory area and marks it as "not accessible".
 *
 * @param p pointer to the memory area
 * @param length number of bytes to poison
 */
static inline void
PoisonInaccessible(gcc_unused void *p, gcc_unused size_t size)
{
#ifdef POISON
	memset(p, 0x01, size);
#endif
}

/**
 * Poisons the specified memory area and marks it as "not defined".
 *
 * @param p pointer to the memory area
 * @param length number of bytes to poison
 */
static inline void
PoisonUndefined(gcc_unused void *p, gcc_unused size_t size)
{
#ifdef POISON
	memset(p, 0x02, size);
#endif
}

#endif
