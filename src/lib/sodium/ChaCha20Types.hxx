// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <array>
#include <cstddef> // for std::byte
#include <span>

// 32 = crypto_stream_chacha20_KEYBYTES
using ChaCha20Key = std::array<std::byte, 32>;
using ChaCha20KeyPtr = std::span<std::byte, 32>;
using ChaCha20KeyView = std::span<const std::byte, 32>;

// 8 = crypto_stream_chacha20_NONCEBYTES
using ChaCha20Nonce = std::array<std::byte, 8>;
using ChaCha20NoncePtr = std::span<std::byte, 8>;
using ChaCha20NonceView = std::span<const std::byte, 8>;
