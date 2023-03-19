// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "InvokeTask.hxx"
#include "util/StaticCache.hxx"
#include "util/Concepts.hxx"
#include "util/IntrusiveList.hxx"

#include <memory>
#include <optional>

namespace Co {

/**
 * A cache which handles multiple concurrent requests on the same key
 * and provides a coroutine interface for both the factory and the
 * getter method.
 *
 * @param Factory a factory class whose operator() returns a Coroutine
 * promise; it may optionally have a method `bool IsCacheable(const
 * Data&) const`
 */
template<typename Factory, typename Key, typename Data,
	 std::size_t max_size,
	 std::size_t table_size,
	 typename Hash=std::hash<Key>,
	 typename Equal=std::equal_to<Key>>
class Cache : Factory {
	template<typename F, typename=void>
	struct IsCacheable {
		constexpr bool operator()(const F &, const Data &) noexcept {
			return true;
		}
	};

	template<typename F>
	struct IsCacheable<F, std::void_t<decltype(std::declval<F>().IsCacheable(std::declval<Data>()))>> {
		constexpr bool operator()(const F &factory, const Data &data) noexcept {
			return factory.IsCacheable(data);
		}
	};

	using Cache_ = StaticCache<Key, Data, max_size, table_size, Hash, Equal>;
	Cache_ cache;

	struct Request;

	struct Handler : IntrusiveListHook<IntrusiveHookMode::NORMAL> {
		Request *request = nullptr;

		std::coroutine_handle<> continuation;

		std::optional<Data> data;

		std::exception_ptr error;

		explicit Handler(const Data &_data) noexcept
			:data(_data) {}

		explicit Handler(std::exception_ptr _error) noexcept
			:error(std::move(_error)) {}

		explicit Handler(Request &_request) noexcept
			:request(&_request)
		{
			_request.handlers.push_back(*this);
		}

		~Handler() noexcept {
			if (request) {
				unlink();

				if (request->IsAbandoned())
					delete request;
			}
		}

		void Resume() noexcept {
			assert(request == nullptr);
			assert(continuation);

			continuation.resume();
		}

		bool IsReady() const noexcept {
			return data || error;
		}

		void AwaitSuspend(std::coroutine_handle<> _continuation) noexcept {
			assert(!IsReady());

			continuation = _continuation;
		}

		Data &&AwaitResume() {
			assert(IsReady());

			request = nullptr;

			if (error)
				std::rethrow_exception(error);

			return std::move(*data);
		}
	};

	struct Task {
		std::unique_ptr<Handler> handler;

		explicit Task(const Data &data) noexcept
			:handler(new Handler(data)) {}

		explicit Task(std::exception_ptr _error) noexcept
			:handler(new Handler(std::move(_error))) {}

		explicit Task(Request &request) noexcept
			:handler(new Handler(request))
		{
		}

		bool IsReady() const noexcept {
			return handler->IsReady();
		}

		void AwaitSuspend(std::coroutine_handle<> _continuation) noexcept {
			handler->AwaitSuspend(_continuation);
		}

		decltype(auto) AwaitResume() {
			return handler->AwaitResume();
		}

		auto operator co_await() noexcept {
			struct Awaitable final {
				Task &task;

				bool await_ready() const noexcept {
					return task.IsReady();
				}

				std::coroutine_handle<>
				await_suspend(std::coroutine_handle<> _continuation) noexcept {
					task.AwaitSuspend(_continuation);
					return std::noop_coroutine();
				}

				decltype(auto) await_resume() {
					return task.AwaitResume();
				}
			};

			return Awaitable{*this};
		}
	};

	struct Request : AutoUnlinkIntrusiveListHook {
		Cache &cache;

		Key key;

		IntrusiveList<Handler> handlers;

		Co::InvokeTask task;

		bool store = true;

		template<typename K>
		Request(Cache &_cache, K &&_key) noexcept
			:cache(_cache), key(std::forward<K>(_key)) {}

		bool IsDone() const noexcept {
			return task.done();
		}

		bool IsAbandoned() const noexcept {
			return handlers.empty();
		}

		void Resume() noexcept {
			handlers.remove_and_dispose_if([](const Handler &handler){
				assert(handler.request != nullptr);
				return !!handler.continuation;
			}, [](Handler *handler) {
				assert(handler->request != nullptr);
				handler->request = nullptr;
				handler->Resume();
			});
		}

		Co::InvokeTask Run(Factory &factory) {
			assert(task);
			assert(!task.done());

			const Key &c_key = key;
			auto value = co_await factory(c_key);
			for (auto &i : handlers)
				i.data.emplace(value);

			if (store && !IsCacheable<Factory>{}(factory, value))
				store = false;

			if (store)
				cache.cache.Put(std::move(key),
						std::move(value));
		}

		void Start(Factory &factory) noexcept {
			assert(!task);
			assert(!handlers.empty());

			task = Run(factory);
			task.Start(BIND_THIS_METHOD(OnCompletion));
		}

		void OnCompletion(std::exception_ptr error) noexcept {
			assert(task);
			assert(task.done());

			if (error)
				for (auto &i : handlers)
					i.error = error;

			Resume();
			delete this;
		}
	};

	IntrusiveList<Request> requests;

public:
	using hasher = typename Cache_::hasher;
	using key_equal = typename Cache_::key_equal;

	template<typename... P>
	explicit Cache(P&&... _params) noexcept
		:Factory(std::forward<P>(_params)...) {}

	decltype(auto) hash_function() const noexcept {
		return cache.hash_function();
	}

	decltype(auto) key_eq() const noexcept {
		return cache.key_eq();
	}

	template<typename K>
	[[gnu::pure]]
	decltype(auto) GetIfCached(K &&key) noexcept {
		return cache.Get(std::forward<K>(key));
	}

	template<typename K>
	Task Get(K &&key) {
		auto *cached = GetIfCached(key);
		if (cached != nullptr)
			return Task(*cached);

		for (auto &i : requests)
			if (i.store && !i.IsDone() && key_eq()(i.key, key))
				return Task(i);

		auto *request = new Request(*this, std::forward<K>(key));
		requests.push_back(*request);
		Task task(*request);
		request->Start(*this);
		return task;
	}

	/**
	 * Delete all cache items and mark all pending requests as
	 * "don't store".
	 */
	void Clear() noexcept {
		cache.Clear();

		for (auto &i : requests)
			i.store = false;
	}

	template<typename K>
	void Remove(K &&key) noexcept {
		for (auto &i : requests)
			if (key_eq()(i.key, key))
				i.store = false;

		cache.Remove(std::forward<K>(key));
	}

	void RemoveIf(Predicate<const Key &, const Data &> auto p) noexcept {
		/* note: this method is unable to check pending
		   requests, so unfortunately, pending requests may
		   result in stale cache items */

		cache.RemoveIf(p);
	}
};

} // namespace Co

