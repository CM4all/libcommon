// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "DeferEvent.hxx"
#include "Loop.hxx"

void
DeferEvent::Schedule() noexcept
{
	if (!IsPending())
		loop.AddDefer(*this);

	assert(IsPending());
}

void
DeferEvent::ScheduleIdle() noexcept
{
	if (!IsPending())
		loop.AddIdle(*this);

	assert(IsPending());
}
