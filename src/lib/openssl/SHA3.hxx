// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "util/FixedString.hxx"

#include <cstddef>
#include <span>

FixedString<64>
EvpSHA3_256_Hex(std::span<const std::byte> input);
