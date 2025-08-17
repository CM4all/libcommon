// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

/**
 * A type-dependent value that is always false.  This common trick is
 * needed in some situations, e.g. to prevent an unreachable
 * static_assert(false) in the last "(constexpr) else" branch to be
 * ill-formed (in a std::visit function to a std::variant to implement
 * a compile-time check for exhaustiveness).
 */
template<class> constexpr bool always_false_v = false;
