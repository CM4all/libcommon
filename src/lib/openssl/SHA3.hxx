// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <array>
#include <span>
#include <string_view>

std::array<char, 64>
EvpSHA3_256_Hex(std::span<const std::byte> input);

template<typename T>
std::array<char, 64>
EvpSHA3_256_Hex(std::span<T> input)
{
	return EvpSHA3_256_Hex(std::as_bytes(input));
}

inline std::array<char, 64>
EvpSHA3_256_Hex(std::string_view input)
{
	return EvpSHA3_256_Hex(std::span{input.data(), input.size()});
}
