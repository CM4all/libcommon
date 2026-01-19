// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <array>
#include <cstddef> // for std::byte
#include <span>

// 32 = crypto_secretbox_KEYBYTES
using CryptoSecretBoxKey = std::array<std::byte, 32>;
using CryptoSecretBoxKeyPtr = std::span<std::byte, 32>;
using CryptoSecretBoxKeyView = std::span<const std::byte, 32>;

// 24 = crypto_secretbox_NONCEBYTES
using CryptoSecretBoxNonce = std::array<std::byte, 24>;
using CryptoSecretBoxNoncePtr = std::span<std::byte, 24>;
using CryptoSecretBoxNonceView = std::span<const std::byte, 24>;
