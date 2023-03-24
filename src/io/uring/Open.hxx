// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Operation.hxx"

#include <sys/types.h> // for mode_t

class FileDescriptor;

namespace Uring {

class Queue;
class OpenHandler;

/**
 * Call openat() with io_uring.  The new file descriptor and file
 * information is passed to the given #OpenHandler on completion.
 */
class Open final : Operation {
	Queue &queue;

	OpenHandler &handler;

	bool canceled = false;

public:
	Open(Queue &_queue, OpenHandler &_handler) noexcept
		:queue(_queue), handler(_handler) {}

	auto &GetQueue() const noexcept {
		return queue;
	}

	void StartOpen(FileDescriptor directory_fd, const char *path,
		       int flags, mode_t mode=0) noexcept;

	void StartOpen(const char *path,
		       int flags, mode_t mode=0) noexcept;

	void StartOpenReadOnly(FileDescriptor directory_fd,
			       const char *path) noexcept;

	void StartOpenReadOnly(const char *path) noexcept;

	/**
	 * Same as StartOpenReadOnly(), but with RESOLVE_BENEATH.
	 */
	void StartOpenReadOnlyBeneath(FileDescriptor directory_fd,
				      const char *path) noexcept;

	/**
	 * Cancel this operation.  This works only if this instance
	 * was allocated on the heap using `new`.  It will be freed
	 * using `delete` after the kernel has finished cancellation,
	 * i.e. the caller resigns ownership.
	 */
	void Cancel() noexcept {
		canceled = true;
	}

private:
	/* virtual methods from class Operation */
	void OnUringCompletion(int res) noexcept override;
};

} // namespace Uring
