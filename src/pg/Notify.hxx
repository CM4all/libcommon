/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef PG_NOTIFY_HXX
#define PG_NOTIFY_HXX

#include "util/Compiler.h"

#include <postgresql/libpq-fe.h>

#include <algorithm>

namespace Pg {

/**
 * A thin C++ wrapper for a PGnotify pointer.
 */
class Notify {
	PGnotify *notify;

public:
	Notify():notify(nullptr) {}
	explicit Notify(PGnotify *_notify):notify(_notify) {}

	Notify(const Notify &other) = delete;
	Notify(Notify &&other):notify(other.notify) {
		other.notify = nullptr;
	}

	~Notify() {
		if (notify != nullptr)
			PQfreemem(notify);
	}

	operator bool() const {
		return notify != nullptr;
	}

	Notify &operator=(const Notify &other) = delete;
	Notify &operator=(Notify &&other) {
		std::swap(notify, other.notify);
		return *this;
	}

	const PGnotify &operator*() const {
		return *notify;
	}

	const PGnotify *operator->() const {
		return notify;
	}
};

} /* namespace Pg */

#endif
