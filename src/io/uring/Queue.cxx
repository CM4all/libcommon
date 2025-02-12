// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Queue.hxx"
#include "CancellableOperation.hxx"
#include "util/DeleteDisposer.hxx"

#include <stdexcept>

namespace Uring {

Queue::Queue(unsigned entries, unsigned flags)
	:ring(entries, flags)
{
}

Queue::Queue(unsigned entries, struct io_uring_params &params)
	:ring(entries, params)
{
}

Queue::~Queue() noexcept
{
	operations.clear_and_dispose(DeleteDisposer{});
}

struct io_uring_sqe &
Queue::RequireSubmitEntry()
{
	auto *sqe = GetSubmitEntry();
	if (sqe == nullptr) {
		/* the submit queue is full; submit it to the kernel
		   and try again */
		ring.Submit();

		sqe = GetSubmitEntry();
		if (sqe == nullptr)
			throw std::runtime_error{"io_uring_get_sqe() failed"};
	}

	return *sqe;
}

void
Queue::AddPending(struct io_uring_sqe &sqe,
		  Operation &operation) noexcept
{
	auto *c = new CancellableOperation(operation);
	operations.push_back(*c);
	io_uring_sqe_set_data(&sqe, c);
}

inline void
Queue::_DispatchOneCompletion(const struct io_uring_cqe &cqe) noexcept
{
	void *data = io_uring_cqe_get_data(&cqe);
	if (data != nullptr) {
		auto *c = (CancellableOperation *)data;
		const bool more = cqe.flags & IORING_CQE_F_MORE;
		c->OnUringCompletion(cqe.res, more);
		if (!more) {
			c->unlink();
			delete c;
		}
	}
}

void
Queue::DispatchOneCompletion(struct io_uring_cqe &cqe) noexcept
{
	_DispatchOneCompletion(cqe);
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

inline unsigned
Queue::DispatchCompletions(struct io_uring_cqe &_cqe) noexcept
{
	return ring.VisitCompletions(&_cqe, [](const struct io_uring_cqe &cqe){
		_DispatchOneCompletion(cqe);
	});
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

bool
Queue::SubmitAndWaitDispatchCompletions(struct __kernel_timespec *timeout)
{
	auto *cqe = ring.SubmitAndWaitCompletion(timeout);
	if (cqe == nullptr)
		return false;

	return DispatchCompletions(*cqe) > 0;
}

} // namespace Uring
