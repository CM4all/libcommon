// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "event/FineTimerEvent.hxx"

namespace Systemd {

/**
 * This class implements the systemd watchdog protocol; see
 * systemd.service(5) and sd_watchdog_enabled(3).  If the watchdog is
 * not enabled, this class does nothing.
 */
class Watchdog {
	FineTimerEvent timer;

	Event::Duration interval;

public:
	explicit Watchdog(EventLoop &_loop) noexcept;

	void Disable() noexcept {
		timer.Cancel();
	}

private:
	void OnTimer() noexcept;
};

} // namespace Systemd
