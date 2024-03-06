// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <array>
#include <cstddef> // for std::byte
#include <span>

// 32 = crypto_sign_PUBLICKEYBYTES
using CryptoSignPublicKey = std::array<std::byte, 32>;
using CryptoSignPublicKeyBuffer = std::span<std::byte, 32>;
using CryptoSignPublicKeyView = std::span<const std::byte, 32>;

// 64 = crypto_sign_SECRETKEYBYTES
using CryptoSignSecretKey = std::array<std::byte, 64>;
using CryptoSignSecretKeyBuffer = std::span<std::byte, 64>;
using CryptoSignSecretKeyView = std::span<const std::byte, 64>;
