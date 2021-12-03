/*
 * Copyright 2020-2021 CM4all GmbH
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

#include "Operation.hxx"
#include "co/Compat.hxx"

#include <cstddef>

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

	struct Awaitable final {
		CoOperation &op;

		bool await_ready() const noexcept {
			return !op.IsUringPending();
		}

		std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept {
			op.continuation = continuation;
			return std::noop_coroutine();
		}

		decltype(auto) await_resume() const {
			return op.GetValue();
		}
	};

	Awaitable operator co_await() noexcept {
		return {*this};
	}

private:
	decltype(auto) GetValue() noexcept {
		return base.GetValue(result);
	}
};

class CoStatxOperation final : public CoOperationBase {
	struct statx stx;

public:
	CoStatxOperation(struct io_uring_sqe &sqe,
			 FileDescriptor directory_fd, const char *path,
			 int flags, unsigned mask) noexcept;

	const struct statx &GetValue(int value);
};

using CoStatx = CoOperation<CoStatxOperation>;

class CoOpenOperation final : public CoOperationBase {
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

class CoCloseOperation final : public CoOperationBase {
public:
	CoCloseOperation(struct io_uring_sqe &sqe, FileDescriptor fd) noexcept;

	void GetValue(int value);
};

using CoClose = CoOperation<CoCloseOperation>;

class CoReadOperation final {
public:
	CoReadOperation(struct io_uring_sqe &sqe, FileDescriptor fd,
			void *buffer, std::size_t size,
			off_t offset, int flags=0) noexcept;

	std::size_t GetValue(int value) const;
};

using CoRead = CoOperation<CoReadOperation>;

class CoWriteOperation final {
public:
	CoWriteOperation(struct io_uring_sqe &sqe, FileDescriptor fd,
			 const void *buffer, std::size_t size,
			 off_t offset, int flags=0) noexcept;

	std::size_t GetValue(int value) const;
};

using CoWrite = CoOperation<CoWriteOperation>;

} // namespace Uring
