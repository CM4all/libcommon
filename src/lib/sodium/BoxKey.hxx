// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <array>
#include <cstddef> // for std::byte
#include <span>

// 32 = crypto_box_PUBLICKEYBYTES
using CryptoBoxPublicKey = std::array<std::byte, 32>;
using CryptoBoxPublicKeyBuffer = std::span<std::byte, 32>;
using CryptoBoxPublicKeyView = std::span<const std::byte, 32>;

// 32 = crypto_box_SECRETKEYBYTES
using CryptoBoxSecretKey = std::array<std::byte, 32>;
using CryptoBoxSecretKeyBuffer = std::span<std::byte, 32>;
using CryptoBoxSecretKeyView = std::span<const std::byte, 32>;
