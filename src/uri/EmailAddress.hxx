// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <string_view>

/**
 * Is this a valid email address according to RFC 5322 3.4.1?
 *
 * @see https://datatracker.ietf.org/doc/html/rfc5322#section-3.4.1
 */
[[gnu::pure]]
bool
VerifyEmailAddress(std::string_view name) noexcept;
