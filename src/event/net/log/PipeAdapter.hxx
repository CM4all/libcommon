// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "net/log/Datagram.hxx"
#include "event/PipeLineReader.hxx"
#include "util/TokenBucket.hxx"

namespace Net::Log {

class Sink;

/**
 * Reads lines from a pipe and sends them to a Pond server.  This can
 * be useful as an adapter for a child process's stderr.
 *
 * If the pipe ends or fails, there is no callback/notification.  This
 * class just unregisters the event and stops operating.
 */
class PipeAdapter final : PipeLineReaderHandler {
	PipeLineReader line_reader;

	Sink &sink;

	Datagram datagram;

	TokenBucket token_bucket;

	double rate_limit = -1, burst;

public:
	/**
	 * @param _pipe the pipe this class will read lines from
	 * @param _socket a datagram socket connected to a Pond server
	 * (owned by caller)
	 */
	PipeAdapter(EventLoop &event_loop, UniqueFileDescriptor _pipe,
		    Sink &_sink, Net::Log::Type type) noexcept
		:line_reader(event_loop, std::move(_pipe), *this),
		 sink(_sink),
		 datagram{.type=type} {}

	EventLoop &GetEventLoop() const noexcept {
		return line_reader.GetEventLoop();
	}

	/**
	 * Returns a reference to the #Datagram instance.  The caller
	 * may modify it.  New pointed-to data is owned by the caller
	 * and must be kept alive/valid as long as this #PipeAdapter
	 * lives.
	 */
	Datagram &GetDatagram() noexcept {
		return datagram;
	}

	void SetRateLimit(double _rate_limit, double _burst) noexcept {
		rate_limit = _rate_limit;
		burst = _burst;
	}

	void Flush() noexcept {
		line_reader.Flush();
	}

private:
	/* virtual methods from class PipeLineReaderHandler */
	bool OnPipeLine(std::span<char> line) noexcept override;
	void OnPipeEnd() noexcept override;
};

} // namespace Net::Log
