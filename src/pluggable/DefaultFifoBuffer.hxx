// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "util/ForeignFifoBuffer.hxx"

#include <stdint.h>

/**
 * A frontend for #SliceFifoBuffer which allows to replace it with a
 * simple heap-allocated buffer when some client code gets copied to
 * another project.
 */
class DefaultFifoBuffer : public ForeignFifoBuffer<std::byte> {
	static constexpr size_t SIZE = 8192;

public:
	DefaultFifoBuffer() noexcept:ForeignFifoBuffer(nullptr) {}

	~DefaultFifoBuffer() noexcept {
		delete[] GetBuffer();
	}

	bool IsDefinedAndFull() const noexcept {
		return IsDefined() && IsFull();
	}

	void Allocate() noexcept {
		SetBuffer({new std::byte[SIZE], SIZE});
	}

	void Free() noexcept {
		delete[] GetBuffer();
		SetNull();
	}

	void AllocateIfNull() noexcept {
		if (IsNull())
			Allocate();
	}

	void FreeIfDefined() noexcept {
		Free();
	}

	void FreeIfEmpty() noexcept {
		if (empty())
			Free();
	}

	void CycleIfEmpty() noexcept {
	}

	void MoveFromAllowBothNull(DefaultFifoBuffer &src) noexcept {
		if (empty())
			/* optimized special case: swap buffer pointers instead of
			   copying data */
			swap(src);
		else
			MoveFrom(src);
	}
};

/**
 * Perform global initialization for #DefaultFifoBuffer.  An instance
 * of this class should usually be placed in main() before using
 * #DefaultFifoBuffer.
 *
 * This implementation does nothing, but other implementations of
 * #DefaultFifoBuffer may require global initialization.
 */
class ScopeInitDefaultFifoBuffer {};
