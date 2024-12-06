// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Operation.hxx"
#include "co/AwaitableHelper.hxx"

#include <cstddef>
#include <span>

#include <sys/stat.h>
#include <sys/types.h>

struct io_uring_sqe;
class FileDescriptor;
class UniqueFileDescriptor;

namespace Uring {

class Queue;

/**
 * Coroutine integration for an io_uring #Operation.
 */
class CoOperationBase : public Operation {
protected:
	std::coroutine_handle<> continuation;

	int result;

private:
	/* virtual methods from class Uring::Operation */
	void OnUringCompletion(int res) noexcept final;
};

struct io_uring_sqe &
_RequireSubmitEntry(Queue &queue);

void
_Push(Queue &queue, struct io_uring_sqe &sqe, Operation &operation) noexcept;

/**
 * Coroutine integration for an io_uring #Operation.
 */
template<typename T>
class CoOperation final : public CoOperationBase {
	T base;

	using Awaitable = Co::AwaitableHelper<CoOperation<T>, false>;
	friend Awaitable;

	template<typename... Args>
	CoOperation(struct io_uring_sqe &sqe, Queue &queue, Args&&... args)
		:base(sqe, std::forward<Args>(args)...)
	{
		_Push(queue, sqe, *this);
	}

public:
	template<typename... Args>
	CoOperation(Queue &queue, Args&&... args)
		:CoOperation(_RequireSubmitEntry(queue), queue,
			     std::forward<Args>(args)...) {}

	Awaitable operator co_await() noexcept {
		return *this;
	}

private:
	bool IsReady() const noexcept {
		return !IsUringPending();
	}

	decltype(auto) TakeValue() noexcept(noexcept(base.GetValue(result))) {
		return base.GetValue(result);
	}
};

class CoStatxOperation final {
	struct statx stx;

public:
	CoStatxOperation(struct io_uring_sqe &sqe,
			 FileDescriptor directory_fd, const char *path,
			 int flags, unsigned mask) noexcept;

	const struct statx &GetValue(int value);
};

using CoStatx = CoOperation<CoStatxOperation>;

class CoOpenOperation final {
public:
	CoOpenOperation(struct io_uring_sqe &sqe,
			FileDescriptor directory_fd, const char *path,
			int flags, mode_t mode) noexcept;

	UniqueFileDescriptor GetValue(int value);
};

CoOperation<CoOpenOperation>
CoOpenReadOnly(Queue &queue,
	       FileDescriptor directory_fd, const char *path) noexcept;

CoOperation<CoOpenOperation>
CoOpenReadOnly(Queue &queue, const char *path) noexcept;

class CoCloseOperation final {
public:
	CoCloseOperation(struct io_uring_sqe &sqe, FileDescriptor fd) noexcept;

	void GetValue(int value);
};

using CoClose = CoOperation<CoCloseOperation>;

class CoReadOperation final {
public:
	CoReadOperation(struct io_uring_sqe &sqe, FileDescriptor fd,
			std::span<std::byte> dest,
			off_t offset, int flags=0) noexcept;

	std::size_t GetValue(int value) const;
};

using CoRead = CoOperation<CoReadOperation>;

class CoWriteOperation final {
public:
	CoWriteOperation(struct io_uring_sqe &sqe, FileDescriptor fd,
			 std::span<const std::byte> src,
			 off_t offset, int flags=0) noexcept;

	std::size_t GetValue(int value) const;
};

using CoWrite = CoOperation<CoWriteOperation>;

/**
 * Performs the unlink() or unlinkat() system call.
 *
 * @return 0 on success or a negative errno value on error (no
 * exceptions thrown on error)
 */
class CoUnlinkOperation final {
public:
	CoUnlinkOperation(struct io_uring_sqe &sqe,
			  const char *path,
			  int flags=0) noexcept;

	CoUnlinkOperation(struct io_uring_sqe &sqe,
			  FileDescriptor directory_fd, const char *path,
			  int flags=0) noexcept;

	int GetValue(int value) const noexcept {
		return value;
	}
};

using CoUnlink = CoOperation<CoUnlinkOperation>;

} // namespace Uring
