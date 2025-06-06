// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "Operation.hxx"
#include "util/IntrusiveList.hxx"

#include <cassert>
#include <utility>

#include <errno.h> // for ECANCELED

namespace Uring {

class CancellableOperation
	: public IntrusiveListHook<IntrusiveHookMode::NORMAL>
{
	Operation *operation;

public:
	CancellableOperation(Operation &_operation) noexcept
		:operation(&_operation)
	{
		assert(operation->cancellable == nullptr);
		operation->cancellable = this;
	}

	~CancellableOperation() noexcept {
		if (operation != nullptr)
			operation->OnUringCompletion(-ECANCELED);
	}

	void Cancel(Operation &_operation) noexcept {
		(void)_operation;
		assert(operation == &_operation);

		operation = nullptr;

		// TODO: io_uring_prep_cancel()
	}

	void Replace(Operation &old_operation,
		     Operation &new_operation) noexcept {
		assert(operation == &old_operation);
		assert(old_operation.cancellable == this);

		old_operation.cancellable = nullptr;
		operation = &new_operation;
		new_operation.cancellable = this;
	}

	void OnUringCompletion(int res, bool more) noexcept {
		if (operation == nullptr)
			return;

		assert(operation->cancellable == this);

		if (more) {
			operation->OnUringCompletion(res);
		} else {
			operation->cancellable = nullptr;

			std::exchange(operation, nullptr)->OnUringCompletion(res);
		}
	}
};

} // namespace Uring
