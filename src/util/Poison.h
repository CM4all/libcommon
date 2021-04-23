/*
 * Copyright 2007-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*! \file
 * \brief Memory poisoning.
 *
 * \author Max Kellermann (mk@cm4all.com)
 */

#ifndef POISON_H
#define POISON_H

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
PoisonInaccessible([[maybe_unused]] void *p, [[maybe_unused]] size_t size)
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
PoisonUndefined([[maybe_unused]] void *p, [[maybe_unused]] size_t size)
{
#if defined(HAVE_VALGRIND_MEMCHECK_H) && !defined(NDEBUG)
	(void)VALGRIND_MAKE_MEM_UNDEFINED(p, size);
#endif

#ifdef POISON
	memset(p, 0x02, size);
#endif

#if defined(HAVE_VALGRIND_MEMCHECK_H) && !defined(NDEBUG)
	(void)VALGRIND_MAKE_MEM_UNDEFINED(p, size);
#endif
}

#endif
