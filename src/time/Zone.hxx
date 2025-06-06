// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

/**
 * Determine the time zone offset in a portable way.
 */
[[gnu::const]]
int
GetTimeZoneOffset() noexcept;
