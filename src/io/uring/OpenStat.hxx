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
#include "io/UniqueFileDescriptor.hxx"

#include <exception>

#include <sys/stat.h>

namespace Uring {

class Queue;
class OpenStatHandler;

/**
 * Combined io_uring operation for openat() and statx().  The new file
 * descriptor and file information is passed to the given
 * #OpenStatHandler on completion.
 */
class OpenStat : Operation {
	Queue &queue;

	OpenStatHandler &handler;

	UniqueFileDescriptor fd;

	struct statx st;

public:
	OpenStat(Queue &_queue, OpenStatHandler &_handler) noexcept
		:queue(_queue), handler(_handler) {}

	auto &GetQueue() const noexcept {
		return queue;
	}

	void StartOpenStatReadOnly(FileDescriptor directory_fd,
				   const char *path) noexcept;

	void StartOpenStatReadOnly(const char *path) noexcept;

	/**
	 * Same as StartOpenStatReadOnly(), but with RESOLVE_BENEATH.
	 */
	void StartOpenStatReadOnlyBeneath(FileDescriptor directory_fd,
					  const char *path) noexcept;

private:
	/* virtual methods from class Operation */
	void OnUringCompletion(int res) noexcept override;
};

} // namespace Uring
