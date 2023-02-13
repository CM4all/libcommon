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
	DefaultFifoBuffer():ForeignFifoBuffer(nullptr) {}

	bool IsDefinedAndFull() const {
		return IsDefined() && IsFull();
	}

	void Allocate() {
		SetBuffer(new std::byte[SIZE], SIZE);
	}

	void Free() {
		delete[] GetBuffer();
		SetNull();
	}

	void AllocateIfNull() {
		if (IsNull())
			Allocate();
	}

	void FreeIfDefined() {
		Free();
	}

	void FreeIfEmpty() {
		if (empty())
			Free();
	}

	void CycleIfEmpty() {
	}
};
