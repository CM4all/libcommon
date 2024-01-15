// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string_view>

namespace JWT {

/**
 * Perform a rough syntax check on whether the given string may be a
 * JWT.
 */
[[gnu::pure]]
bool
CheckSyntax(std::string_view jwt) noexcept;

} // namespace JWT
