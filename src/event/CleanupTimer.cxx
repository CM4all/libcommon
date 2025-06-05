// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "CleanupTimer.hxx"

void
CleanupTimer::OnTimer() noexcept
{
	if (callback())
		Enable();
}

void
CleanupTimer::Enable() noexcept
{
	if (!event.IsPending())
		event.Schedule(delay);
}

void
CleanupTimer::Disable() noexcept
{
	event.Cancel();
}
