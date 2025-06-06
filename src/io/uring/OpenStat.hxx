// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Operation.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <sys/stat.h>

struct FileAt;

namespace Uring {

class Queue;
class OpenStatHandler;

/**
 * Combined io_uring operation for openat() and statx().  The new file
 * descriptor and file information is passed to the given
 * #OpenStatHandler on completion.
 */
class OpenStat final : Operation {
	Queue &queue;

	OpenStatHandler &handler;

	UniqueFileDescriptor fd;

	struct statx st;

	bool canceled = false;

public:
	OpenStat(Queue &_queue, OpenStatHandler &_handler) noexcept
		:queue(_queue), handler(_handler) {}

	auto &GetQueue() const noexcept {
		return queue;
	}

	void StartOpenStat(FileAt file, int flags, mode_t mode=0) noexcept;

	void StartOpenStat(const char *path,
			   int flags, mode_t mode=0) noexcept;

	void StartOpenStatReadOnly(FileAt file) noexcept;

	void StartOpenStatReadOnly(const char *path) noexcept;

	/**
	 * Same as StartOpenStatReadOnly(), but with RESOLVE_BENEATH.
	 */
	void StartOpenStatReadOnlyBeneath(FileAt file) noexcept;

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
