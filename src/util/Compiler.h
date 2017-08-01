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

/**
 * Are we building with the specified version of gcc (not clang or any
 * other compiler) or newer?
 */
#define GCC_CHECK_VERSION(major, minor) \
	(!defined(__clang__) && \
	 GCC_VERSION >= GCC_MAKE_VERSION(major, minor, 0))

/**
 * Are we building with clang (any version) or at least the specified
 * gcc version?
 */
#define CLANG_OR_GCC_VERSION(major, minor) \
	(defined(__clang__) || GCC_CHECK_VERSION(major, minor))

/**
 * Are we building with gcc (not clang or any other compiler) and a
 * version older than the specified one?
 */
#define GCC_OLDER_THAN(major, minor) \
	(GCC_VERSION > 0 && !defined(__clang__) && \
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

/* pre-0.3 compatibility macros */

#define __attr_always_inline gcc_always_inline
#define __attr_malloc gcc_malloc
#define __attr_pure gcc_pure
#define __attr_const gcc_const
#define __attr_unused gcc_unused
#define __attr_deprecated gcc_deprecated
#define __attr_packed gcc_packed
#define __attr_noreturn gcc_noreturn
#define __attr_sentinel(n) gcc_sentinel(n)
#define __attr_printf(string_index, first_to_check) gcc_printf(string_index, first_to_check)
#define likely(x) gcc_likely(x)
#define unlikely(x) gcc_unlikely(x)

#endif
