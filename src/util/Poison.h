/*! \file
 * \brief Memory poisoning.
 *
 * \author Max Kellermann (mk@cm4all.com)
 */

#ifndef POISON_H
#define POISON_H

#include "Compiler.h"

#if defined(HAVE_VALGRIND_MEMCHECK_H) && !defined(NDEBUG)
#include <valgrind/memcheck.h>
#endif

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
#if defined(HAVE_VALGRIND_MEMCHECK_H) && !defined(NDEBUG)
	(void)VALGRIND_MAKE_MEM_UNDEFINED(p, size);
#endif

#ifdef POISON
	memset(p, 0x01, size);
#endif

#if defined(HAVE_VALGRIND_MEMCHECK_H) && !defined(NDEBUG)
	(void)VALGRIND_MAKE_MEM_NOACCESS(p, size);
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
#if defined(HAVE_VALGRIND_MEMCHECK_H) && !defined(NDEBUG)
	(void)VALGRIND_MAKE_MEM_NOACCESS(p, size);
#endif

#ifdef POISON
	memset(p, 0x02, size);
#endif

#if defined(HAVE_VALGRIND_MEMCHECK_H) && !defined(NDEBUG)
	(void)VALGRIND_MAKE_MEM_NOACCESS(p, size);
#endif
}

#endif
