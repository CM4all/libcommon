// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstdint>

enum class PutAction : uint8_t {
	DESTROY,
	REUSE,
};
