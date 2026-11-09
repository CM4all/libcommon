// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <array>
#include <cstddef> // for std::byte
#include <span>

// 1184 = crypto_kem_mlkem768_PUBLICKEYBYTES
using MlKem768PublicKey = std::array<std::byte, 1184>;
using MlKem768PublicKeyPtr = std::span<std::byte, 1184>;
using MlKem768PublicKeyView = std::span<const std::byte, 1184>;

// 2400 = crypto_kem_mlkem768_SECRETKEYBYTES
using MlKem768SecretKey = std::array<std::byte, 2400>;
using MlKem768SecretKeyPtr = std::span<std::byte, 2400>;
using MlKem768SecretKeyView = std::span<const std::byte, 2400>;

// 1088 = crypto_kem_mlkem768_CIPHERTEXTBYTES
using MlKem768CipherText = std::array<std::byte, 1088>;
using MlKem768CipherTextPtr = std::span<std::byte, 1088>;
using MlKem768CipherTextView = std::span<const std::byte, 1088>;

// 32 = crypto_kem_mlkem768_SHAREDSECRETBYTES
using MlKem768SharedSecret = std::array<std::byte, 32>;
using MlKem768SharedSecretPtr = std::span<std::byte, 32>;
using MlKem768SharedSecretView = std::span<const std::byte, 32>;
