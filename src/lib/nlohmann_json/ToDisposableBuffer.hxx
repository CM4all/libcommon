// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <nlohmann/json_fwd.hpp>

class DisposableBuffer;

namespace Json {

[[gnu::pure]]
DisposableBuffer
ToDisposableBuffer(const nlohmann::json &j) noexcept;

} // namespace Json
