// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Compat.hxx"
#include "util/IntrusiveList.hxx"

#include <cassert>

namespace Co {

/**
 * An awaitable that can be awaited on by multiple waiters.  It is
 * never "ready", it always suspends waiters.  All waiters can be
 * resumed with one method call.
 *
 * This object must remain valid until all waiters have been resumed.
 *
 * This is similar to #MultiAwaitable, but there is no internal task.
 */
class MultiResume final {
	struct Awaitable final : IntrusiveListHook<IntrusiveHookMode::TRACK> {
		MultiResume &multi;

		std::coroutine_handle<> continuation;

		[[nodiscard]]
		explicit Awaitable(MultiResume &_multi) noexcept
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

			/* never ready, always suspend - ResumeAll()
			   will resume the continuation */
			return false;
		}

		void await_suspend(std::coroutine_handle<> _continuation) noexcept {
			assert(!is_linked());
			assert(!continuation);
			assert(_continuation);
			assert(!_continuation.done());

			continuation = _continuation;

			multi.AddRequest(*this);
		}

		void await_resume() noexcept {
			assert(!is_linked());
		}
	};

	/**
	 * A list of suspended waiters.
	 */
	IntrusiveList<Awaitable> requests;

public:
	[[nodiscard]]
	MultiResume() noexcept = default;

	/**
	 * Creates a new awaitable
	 */
	[[nodiscard]]
	auto operator co_await() noexcept {
		return Awaitable{*this};
	}

	void ResumeAll() noexcept {
		/* move the request list to the stack because waiters
		   added later shall not be resumed in this call */
		auto tmp = std::move(requests);

		tmp.clear_and_dispose([](Awaitable *r){
			assert(r->continuation);
			assert(!r->continuation.done());

			r->continuation.resume();
		});
	}

private:
	void AddRequest(Awaitable &r) noexcept {
		requests.push_back(r);
	}
};

} // namespace Co
