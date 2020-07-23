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

#include "Queue.hxx"
#include "CancellableOperation.hxx"
#include "util/DeleteDisposer.hxx"

namespace Uring {

Queue::Queue(unsigned entries, unsigned flags)
	:ring(entries, flags)
{
}

Queue::~Queue() noexcept
{
	operations.clear_and_dispose(DeleteDisposer{});
}

void
Queue::AddPending(struct io_uring_sqe &sqe,
		  Operation &operation) noexcept
{
	auto *c = new CancellableOperation(operation);
	operations.push_back(*c);
	io_uring_sqe_set_data(&sqe, c);
}

void
Queue::DispatchOneCompletion(struct io_uring_cqe &cqe) noexcept
{
	void *data = io_uring_cqe_get_data(&cqe);
	if (data != nullptr) {
		auto *c = (CancellableOperation *)data;
		c->OnUringCompletion(cqe.res);
		operations.erase_and_dispose(operations.iterator_to(*c),
					     DeleteDisposer{});
	}

	ring.SeenCompletion(cqe);
}

bool
Queue::DispatchOneCompletion()
{
	auto *cqe = ring.PeekCompletion();
	if (cqe == nullptr)
		return false;

	DispatchOneCompletion(*cqe);
	return true;
}

bool
Queue::WaitDispatchOneCompletion()
{
	auto *cqe = ring.WaitCompletion();
	if (cqe == nullptr)
		return false;

	DispatchOneCompletion(*cqe);
	return true;
}

} // namespace Uring
