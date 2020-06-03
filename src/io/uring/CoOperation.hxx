/*
 * Copyright 2020 CM4all GmbH
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
public:
	std::coroutine_handle<> continuation;

protected:
	int value;

private:
	/* virtual methods from class Uring::Operation */
	void OnUringCompletion(int res) noexcept override;
};

template<typename Op>
struct CoAwaitable final {
	Op &op;

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

class CoStatxOperation final : public CoOperationBase {
	struct statx stx;

public:
	CoStatxOperation(struct io_uring_sqe *s,
			 FileDescriptor directory_fd, const char *path,
			 int flags, unsigned mask) noexcept;

	auto operator co_await() noexcept {
		return CoAwaitable<CoStatxOperation>{*this};
	}

	const struct statx &GetValue();
};

CoStatxOperation
CoStatx(Queue &queue,
	FileDescriptor directory_fd, const char *path,
	int flags, unsigned mask) noexcept;

class CoOpenOperation final : public CoOperationBase {
public:
	auto operator co_await() noexcept {
		return CoAwaitable<CoOpenOperation>{*this};
	}

	UniqueFileDescriptor GetValue();
};

CoOpenOperation
CoOpenReadOnly(Queue &queue,
	       FileDescriptor directory_fd, const char *path) noexcept;

CoOpenOperation
CoOpenReadOnly(Queue &queue, const char *path) noexcept;

class CoCloseOperation final : public CoOperationBase {
public:
	auto operator co_await() noexcept {
		return CoAwaitable<CoCloseOperation>{*this};
	}

	void GetValue();
};

CoCloseOperation
CoClose(Queue &queue, FileDescriptor fd) noexcept;

class CoReadOperation final : public CoOperationBase {
public:
	auto operator co_await() noexcept {
		return CoAwaitable<CoReadOperation>{*this};
	}

	size_t GetValue() const;
};

CoReadOperation
CoRead(Queue &queue, FileDescriptor fd, void *buffer, size_t size,
       off_t offset) noexcept;

class CoWriteOperation final : public CoOperationBase {
public:
	auto operator co_await() noexcept {
		return CoAwaitable<CoWriteOperation>{*this};
	}

	size_t GetValue() const;
};

CoWriteOperation
CoWrite(Queue &queue, FileDescriptor fd, const void *buffer, size_t size,
	off_t offset) noexcept;

} // namespace Uring
