// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cmath> // for std::log()
#include <concepts> // for std::unsigned_integral
#include <limits> // for std::numeric_limits

namespace RendezvousHashing {

/**
 * Convert a quasi-random unsigned 64 bit integer to a
 * double-precision float in the range 0..1, preserving as many bits
 * as possible.  The returned value has no arithmetic meaning; the
 * goal of this function is only to convert a hash value to a floating
 * point value.
 */
template<std::unsigned_integral I>
static constexpr double
UintToDouble(const I i) noexcept
{
	constexpr unsigned SRC_BITS = std::numeric_limits<I>::digits;

	/* the mantissa has 53 bits, and this is how many bits we can
	   preserve in the conversion */
	constexpr unsigned DEST_BITS = std::numeric_limits<double>::digits;

	if constexpr (DEST_BITS < SRC_BITS) {
		/* discard upper bits that don't fit into the mantissa */
		constexpr uint_least64_t mask = (~I{}) >> (SRC_BITS - DEST_BITS);
		constexpr double max = I{1} << DEST_BITS;

		return (i & mask) / max;
	} else {
		/* don't discard anything */
		static_assert(std::numeric_limits<uintmax_t>::digits > std::numeric_limits<I>::digits);
		constexpr double max = std::uintmax_t{1} << SRC_BITS;

		return i / max;
	}
}

} // namespace RendezvousHashing
