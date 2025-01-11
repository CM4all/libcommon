// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

namespace Net::Log {

struct Datagram;

/**
 * An abstract interface for loggers.
 */
class Sink {
public:
	virtual void Log(const Datagram &d) noexcept = 0;
};

} // namespace Net::Log
