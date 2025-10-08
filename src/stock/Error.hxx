// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <stdexcept>

class StockOverloadedError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};
