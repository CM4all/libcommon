// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <array>
#include <cstddef> // for std::byte
#include <span>

// 32 = crypto_scalarmult_curve25519_BYTES
using Curve25519 = std::array<std::byte, 32>;
using Curve25519Ptr = std::span<std::byte, 32>;
using Curve25519View = std::span<const std::byte, 32>;

// 32 = crypto_scalarmult_curve25519_SCALARBYTES
using Curve25519Scalar = std::array<std::byte, 32>;
using Curve25519ScalarPtr = std::span<std::byte, 32>;
using Curve25519ScalarView = std::span<const std::byte, 32>;
