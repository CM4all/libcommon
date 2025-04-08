// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "UniqueHandle.hxx"
#include "util/IntrusiveList.hxx"

#include <cassert>

namespace Co {

/**
 * A mutex implementation for coroutines: only one coroutine can own
 * the lock.
 */
class Mutex final {
	/**
	 * This class holds the lock (RAII).  It is returned by
	 * #Awaitable.
	 */
	class Lock {
		Mutex &mutex;

	public:
		Lock(Mutex &_mutex) noexcept
			:mutex(_mutex) {
			mutex.SetLocked();
		}

		~Lock() noexcept {
			/* unlock the mutex - this will resume the
			   next waiter */
			mutex.Unlock();
		}

		Lock(const Lock &) = delete;
		Lock &operator=(const Lock &) = delete;
	};

	struct Awaitable final : IntrusiveListHook<IntrusiveHookMode::AUTO_UNLINK> {
		Mutex &mutex;

		std::coroutine_handle<> continuation;

		[[nodiscard]]
		explicit Awaitable(Mutex &_mutex) noexcept
			:mutex(_mutex) {}

		Awaitable(const Awaitable &) = delete;
		Awaitable &operator=(const Awaitable &) = delete;

		[[nodiscard]]
		bool await_ready() const noexcept {
			assert(!is_linked());
			assert(!continuation);

			return !mutex.IsLocked();
		}

		void await_suspend(std::coroutine_handle<> _continuation) noexcept {
			assert(!is_linked());
			assert(!continuation);
			assert(_continuation);
			assert(!_continuation.done());

			continuation = _continuation;

			mutex.AddRequest(*this);
		}

		[[nodiscard]]
		Lock await_resume() noexcept {
			assert(!is_linked());
			assert(!mutex.IsLocked());

			return Lock{mutex};
		}
	};

	/**
	 * A list of suspended waiters.
	 */
	IntrusiveList<Awaitable> requests;

	bool locked = false;

public:
	[[nodiscard]]
	Mutex() noexcept = default;

	/**
	 * Attempt to lock the mutex.
	 *
	 * @return a #Lock object which owns the lock
	 */
	[[nodiscard]]
	auto operator co_await() noexcept {
		return Awaitable{*this};
	}

private:
	void AddRequest(Awaitable &awaitable) noexcept {
		assert(locked);

		requests.push_back(awaitable);
	}

	bool IsLocked() const noexcept {
		return locked;
	}

	void SetLocked() noexcept {
		assert(!locked);

		locked = true;
	}

	void Unlock() noexcept {
		assert(locked);
		locked = false;

		if (!requests.empty()) {
			requests.pop_front_and_dispose([](auto *awaitable){
				awaitable->continuation.resume();
			});
		}
	}
};

} // namespace Co
