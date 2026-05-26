// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "RegexPointer.hxx"
#include "util/SharedLease.hxx"

namespace Pcre {

/**
 * A #RegexPointer whose ownership is managed by #SharedLease.
 */
class SharedRegex : public RegexPointer {
	SharedLease lease;

public:
	SharedRegex() = default;

	[[nodiscard]]
	explicit SharedRegex(RegexPointer r, SharedLease &&_lease) noexcept
		:RegexPointer(r), lease(std::move(_lease)) {}

	[[nodiscard]]
	explicit SharedRegex(RegexPointer r, SharedAnchor &_anchor) noexcept
		:RegexPointer(r), lease(_anchor) {}

	SharedRegex(SharedRegex &&src) noexcept = default;
	SharedRegex &operator=(SharedRegex &&src) noexcept = default;
};

} // namespace Pcre

