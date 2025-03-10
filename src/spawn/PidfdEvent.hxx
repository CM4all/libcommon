// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "event/PipeEvent.hxx"
#include "event/Chrono.hxx"
#include "io/Logger.hxx"

class ExitListener;
class UniqueFileDescriptor;

class PidfdEvent final {
	const Logger logger;

	const Event::TimePoint start_time;

	PipeEvent event;

	ExitListener *listener;

public:
	PidfdEvent(EventLoop &event_loop,
		   UniqueFileDescriptor &&_pidfd,
		   std::string_view _name,
		   ExitListener &_listener) noexcept;

	~PidfdEvent() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	const auto &GetLogger() const noexcept {
		return logger;
	}

	bool IsDefined() const noexcept {
		return event.IsDefined();
	}

	void SetListener(ExitListener &_listener) noexcept {
		listener = &_listener;
	}

	/**
	 * @return true on success
	 */
	bool Kill(int signo) noexcept;

private:
	void OnPidfdReady(unsigned) noexcept;
};
