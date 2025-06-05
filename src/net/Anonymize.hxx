// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <string_view>
#include <utility>

[[gnu::pure]]
std::pair<std::string_view, std::string_view>
AnonymizeAddress(std::string_view value) noexcept;
