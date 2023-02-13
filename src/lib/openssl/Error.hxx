// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * OpenSSL error reporting.
 */

#pragma once

#include <stdexcept>
#include <string_view>

class SslError : public std::runtime_error {
public:
	explicit SslError(std::string_view msg={}) noexcept;
};
