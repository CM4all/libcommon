// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <boost/json/fwd.hpp>

class DisposableBuffer;

namespace Json {

[[gnu::pure]]
DisposableBuffer
ToDisposableBuffer(const boost::json::value &v) noexcept;

} // namespace Json
