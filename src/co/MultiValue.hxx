// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Compat.hxx"
#include "util/IntrusiveList.hxx"

#include <cassert>
#include <optional>

namespace Co {

/**
 * An awaitable that can be awaited on by multiple waiters.  As soon
 * as a value is set, it becomes ready and resumes all waiters.
 *
 * This object must remain valid until all waiters have been resumed
 * (or canceled).
 *
 * This is similar to #MultiAwaitable, but there is a return value and it
 * cannot be reused.
 */
template<typename T>
class MultiValue final {
	struct Awaitable final : IntrusiveListHook<IntrusiveHookMode::TRACK> {
		MultiValue &multi;

		std::coroutine_handle<> continuation;

		[[nodiscard]]
		explicit Awaitable(MultiValue &_multi) noexcept
			:multi(_multi) {}

		~Awaitable() noexcept {
			if (is_linked()) {
				assert(continuation);
				assert(!continuation.done());

				unlink();
			}
		}

		Awaitable(const Awaitable &) = delete;
		Awaitable &operator=(const Awaitable &) = delete;

		[[nodiscard]]
		bool await_ready() const noexcept {
			assert(!is_linked());
			assert(!continuation);

			return multi.value.has_value();
		}

		void await_suspend(std::coroutine_handle<> _continuation) noexcept {
			assert(!is_linked());
			assert(!multi.value);
			assert(!continuation);
			assert(_continuation);
			assert(!_continuation.done());

			continuation = _continuation;

			multi.waiters.push_back(*this);
		}

		T await_resume() noexcept {
			assert(!is_linked());
			assert(multi.value);

			return *multi.value;
		}
	};

	/**
	 * A list of suspended waiters.
	 */
	IntrusiveList<Awaitable> waiters;

	std::optional<T> value;

public:
	[[nodiscard]]
	MultiValue() noexcept = default;

	~MultiValue() noexcept {
		assert(waiters.empty());
	}

	/**
	 * Creates a new awaitable
	 */
	[[nodiscard]]
	auto operator co_await() noexcept {
		return Awaitable{*this};
	}

	template<typename U>
	void SetReady(U &&_value) noexcept {
		assert(!value);

		value.emplace(std::forward<U>(_value));

		/* move the request list to the stack because the last
		 * resume() call may destruct this object */
		auto tmp = std::move(waiters);

		tmp.clear_and_dispose([](Awaitable *r){
			assert(r->continuation);
			assert(!r->continuation.done());

			r->continuation.resume();
		});
	}
};

} // namespace Co
