// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstdint>
#include <string_view>

enum class TranslationCommand : uint16_t;

/**
 * Convert a TranslationCommand to its string representation.
 * Returns an empty string_view for unknown commands.
 */
[[gnu::const]]
std::string_view
ToString(TranslationCommand command) noexcept;

/**
 * Parse a string to a TranslationCommand.
 * Returns zero-initialized TranslationCommand for unknown command names.
 */
[[gnu::pure]]
TranslationCommand
ParseTranslationCommand(std::string_view name) noexcept;
