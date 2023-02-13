// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef TIME_CAST_HXX
#define TIME_CAST_HXX

#include <chrono>

template<class Rep, class Period>
constexpr auto
ToFloatSeconds(const std::chrono::duration<Rep,Period> &d) noexcept
{
	return std::chrono::duration_cast<std::chrono::duration<double>>(d).count();
}

#endif
