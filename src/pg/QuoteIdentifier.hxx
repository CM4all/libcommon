// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <string>
#include <string_view>

namespace Pg {

/**
 * Quote a PostgreSQL identifier (doubling embedded double quotes), so
 * a channel name cannot break out of the LISTEN statement.
 */
[[nodiscard]] [[gnu::pure]]
std::string
QuoteIdentifier(std::string_view s) noexcept;

} /* namespace Pg */
