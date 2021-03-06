/*
 * Copyright 2007-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <libpq-fe.h>

#include <algorithm>

namespace Pg {

/**
 * A thin C++ wrapper for a PGnotify pointer.
 */
class Notify {
	PGnotify *notify;

public:
	Notify() noexcept:notify(nullptr) {}
	explicit Notify(PGnotify *_notify) noexcept
		:notify(_notify) {}

	Notify(const Notify &other) = delete;

	Notify(Notify &&other) noexcept
		:notify(other.notify)
	{
		other.notify = nullptr;
	}

	~Notify() noexcept {
		if (notify != nullptr)
			PQfreemem(notify);
	}

	operator bool() const noexcept {
		return notify != nullptr;
	}

	Notify &operator=(const Notify &other) = delete;

	Notify &operator=(Notify &&other) noexcept {
		using std::swap;
		swap(notify, other.notify);
		return *this;
	}

	const PGnotify &operator*() const noexcept {
		return *notify;
	}

	const PGnotify *operator->() const noexcept {
		return notify;
	}
};

} /* namespace Pg */
