// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <array>
#include <cstddef>
#include <span>

std::array<char, 64>
EvpSHA3_256_Hex(std::span<const std::byte> input);
