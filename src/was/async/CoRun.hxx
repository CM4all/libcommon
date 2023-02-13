// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "SimpleHandler.hxx"
#include "co/Task.hxx"

#include <functional>

class EventLoop;

namespace Was {

using CoCallback = std::function<Co::Task<SimpleResponse>(SimpleRequest)>;

void
Run(EventLoop &event_loop, CoCallback handler);

} // namespace Was
