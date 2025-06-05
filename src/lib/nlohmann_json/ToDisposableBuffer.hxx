// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <nlohmann/json_fwd.hpp>

class DisposableBuffer;

namespace Json {

[[gnu::pure]]
DisposableBuffer
ToDisposableBuffer(const nlohmann::json &j) noexcept;

} // namespace Json
