// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "io/uring/Queue.hxx"
#include "event/DeferEvent.hxx"
#include "event/PipeEvent.hxx"

namespace Uring {

class Manager final : public Queue {
	PipeEvent event;

	/**
	 * Responsible for invoking Queue::Submit() only once per
	 * #EventLoop iteration.
	 */
	DeferEvent defer_submit_event;

	bool volatile_event = false;

public:
	explicit Manager(EventLoop &event_loop,
			 unsigned entries=1024, unsigned flags=0);

	void SetVolatile() noexcept {
		volatile_event = true;
		CheckVolatileEvent();
	}

	// virtual methods from class Uring::Queue
	void Submit() override;

private:
	void CheckVolatileEvent() noexcept {
		if (volatile_event && !HasPending())
			event.Cancel();
	}

	void OnReady(unsigned events) noexcept;
	void DeferredSubmit() noexcept;
};

} // namespace Uring
