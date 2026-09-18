// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <avahi-common/strlst.h>

#include <string_view>

namespace Avahi {

/**
 * Extract the value part of the TXT record (i.e. the portion after
 * the "=").  Returns a zero-initialized std::string_view if there is
 * no "=".  (If a value was found, then it is null-terminated.)
 */
[[gnu::pure]]
std::string_view
GetValueFromTxt(const AvahiStringList &src) noexcept;

} // namespace Avahi
