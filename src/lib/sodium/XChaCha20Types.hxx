// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <array>
#include <cstddef> // for std::byte
#include <span>

// 32 = crypto_stream_xchacha20_KEYBYTES
using XChaCha20Key = std::array<std::byte, 32>;
using XChaCha20KeyPtr = std::span<std::byte, 32>;
using XChaCha20KeyView = std::span<const std::byte, 32>;

// 24 = crypto_stream_xchacha20_NONCEBYTES
using XChaCha20Nonce = std::array<std::byte, 24>;
using XChaCha20NoncePtr = std::span<std::byte, 24>;
using XChaCha20NonceView = std::span<const std::byte, 24>;
