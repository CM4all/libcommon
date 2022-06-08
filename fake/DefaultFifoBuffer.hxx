/*
 * Copyright 2007-2022 CM4all GmbH
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
