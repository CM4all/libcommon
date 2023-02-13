// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <libpq-fe.h>

#include <utility>

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
