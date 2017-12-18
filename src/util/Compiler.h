/*
 * Copyright 2007-2017 Content Management AG
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
 * \brief Compiler specific macros.
 *
 * \author Max Kellermann (mk@cm4all.com)
 */

#ifndef COMPILER_H
#define COMPILER_H

#define GCC_MAKE_VERSION(major, minor, patchlevel) ((major) * 10000 + (minor) * 100 + patchlevel)

#ifdef __GNUC__
#define GCC_VERSION GCC_MAKE_VERSION(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#else
#define GCC_VERSION 0
#endif

#ifdef __clang__
#  define CLANG_VERSION GCC_MAKE_VERSION(__clang_major__, __clang_minor__, __clang_patchlevel__)
#elif defined(__GNUC__)
#  define CLANG_VERSION 0
#endif

/**
 * Are we building with the specified version of gcc (not clang or any
 * other compiler) or newer?
 */
#define GCC_CHECK_VERSION(major, minor) \
	(CLANG_VERSION == 0 && \
	 GCC_VERSION >= GCC_MAKE_VERSION(major, minor, 0))

/**
 * Are we building with clang (any version) or at least the specified
 * gcc version?
 */
#define CLANG_OR_GCC_VERSION(major, minor) \
	(CLANG_VERSION > 0 || GCC_CHECK_VERSION(major, minor))

/**
 * Are we building with gcc (not clang or any other compiler) and a
 * version older than the specified one?
 */
#define GCC_OLDER_THAN(major, minor) \
	(GCC_VERSION > 0 && CLANG_VERSION == 0 && \
	 GCC_VERSION < GCC_MAKE_VERSION(major, minor, 0))

#if CLANG_OR_GCC_VERSION(3,0)
#  define gcc_packed __attribute__((packed))
#  define gcc_unused __attribute__((unused))
#  define gcc_warn_unused_result __attribute__((warn_unused_result))
#  define gcc_deprecated __attribute__((deprecated))
#  define gcc_noreturn __attribute__((noreturn))
#  define gcc_always_inline inline __attribute__((always_inline))
#  define gcc_noinline __attribute__((noinline))
#  define gcc_nonnull(...) __attribute__((nonnull(__VA_ARGS__)))
#  define gcc_nonnull_all __attribute__((nonnull))
#  define gcc_returns_nonnull __attribute__((returns_nonnull))
#  define gcc_returns_twice __attribute__((returns_twice))
#  define gcc_malloc __attribute__((malloc))
#  define gcc_pure __attribute__((pure))
#  define gcc_const __attribute__((const))
#  define gcc_sentinel(n) __attribute__((sentinel(n)))
#  define gcc_printf(string_index, first_to_check) __attribute__((format(printf, string_index, first_to_check)))
#  define gcc_likely(x) __builtin_expect(!!(x), 1)
#  define gcc_unlikely(x) __builtin_expect(!!(x), 0)
#  define gcc_aligned(n) __attribute__((aligned(n)))
#  define gcc_visibility_hidden __attribute__((visibility("hidden")))
#  define gcc_visibility_default __attribute__((visibility("default")))
#else
#  define gcc_packed
#  define gcc_unused
#  define gcc_warn_unused_result
#  define gcc_deprecated
#  define gcc_noreturn
#  define gcc_always_inline
#  define gcc_noinline
#  define gcc_nonnull(...)
#  define gcc_nonnull_all
#  define gcc_returns_nonnull
#  define gcc_returns_twice
#  define gcc_malloc
#  define gcc_pure
#  define gcc_const
#  define gcc_sentinel(n)
#  define gcc_printf(string_index, first_to_check)
#  define gcc_likely(x)
#  define gcc_unlikely(x)
#  define gcc_aligned(n)
#  define gcc_visibility_hidden
#  define gcc_visibility_default
#endif

#if CLANG_OR_GCC_VERSION(4,3)
#define gcc_hot __attribute__((hot))
#define gcc_cold __attribute__((cold))
#else
#define gcc_hot
#define gcc_cold
#endif

#if GCC_CHECK_VERSION(4,6)
#define gcc_flatten __attribute__((flatten))
#else
#define gcc_flatten
#endif

#ifndef __cplusplus
/* plain C99 has "restrict" */
#define gcc_restrict restrict
#elif CLANG_OR_GCC_VERSION(3,0)
/* "__restrict__" is a GCC extension for C++ */
#define gcc_restrict __restrict__
#else
/* disable it on other compilers */
#define gcc_restrict
#endif

#ifdef __cplusplus

/* support for C++11 "override" and "final" was added in gcc 4.7 */
#if GCC_OLDER_THAN(4,7)
#define override
#define final
#endif

#if CLANG_OR_GCC_VERSION(4,8)
#define gcc_alignas(T, fallback) alignas(T)
#else
#define gcc_alignas(T, fallback) gcc_aligned(fallback)
#endif

#endif

#if defined(__GNUC__) || defined(__clang__)
#define gcc_unreachable() __builtin_unreachable()
#else
#define gcc_unreachable()
#endif

#endif
