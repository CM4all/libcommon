// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <utility>

#if defined(_LIBCPP_VERSION) && defined(__clang__) && (__clang_major__ < 14 || defined(ANDROID))
/* libc++ until 14 has the coroutine definitions in the
   std::experimental namespace */

#include <experimental/coroutine>

namespace std {
using std::experimental::coroutine_handle;
using std::experimental::suspend_never;
using std::experimental::suspend_always;
using std::experimental::noop_coroutine;
}

#ifdef ANDROID
#if __clang_patchlevel__ >= 6
/* Android NDK r25 emits the warning "support for
   std::experimental::coroutine_traits will be removed in LLVM 15; use
   std::coroutine_traits instead" but std::coroutine_traits is
   missing; disable the warning until we have a proper solution */
#pragma GCC diagnostic ignored "-Wdeprecated-experimental-coroutine"
#endif
#endif

#else /* not clang */

#include <coroutine>
#ifndef __cpp_impl_coroutine
#error Need -fcoroutines
#endif

#endif /* not clang */
