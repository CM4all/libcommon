// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Manager.hxx"
#include "util/PrintException.hxx"

namespace Uring {

void
Manager::OnReady(unsigned) noexcept
{
	try {
		DispatchCompletions();
	} catch (...) {
		PrintException(std::current_exception());
	}

	CheckVolatileEvent();
}

void
Manager::DeferredSubmit() noexcept
{
	try {
		Queue::Submit();
	} catch (...) {
		PrintException(std::current_exception());
	}
}

} // namespace Uring
