// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
#  define gcc_always_inline inline __attribute__((always_inline))
#  define gcc_const __attribute__((const))
#  define gcc_printf(string_index, first_to_check) __attribute__((format(printf, string_index, first_to_check)))
#else
#  define gcc_always_inline
#  define gcc_const
#  define gcc_printf(string_index, first_to_check)
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

#if defined(__GNUC__) || defined(__clang__)
#define gcc_unreachable() __builtin_unreachable()
#else
#define gcc_unreachable()
#endif

#endif
