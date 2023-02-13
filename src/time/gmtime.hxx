// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH

/*
 * Babak's fast gmtime() implementation taken from branch
 * libcm4all-core-1.20.8+psql revision 847.
 */

#pragma once

#include <time.h>

[[gnu::const]]
struct tm
sysx_time_gmtime(time_t t) noexcept;
