/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "event/PipeEvent.hxx"

#include <memory>

class UniqueFileDescriptor;
class DisposableBuffer;

namespace Was {

class Buffer;

class SimpleInputHandler {
public:
	virtual void OnWasInput(DisposableBuffer input) noexcept = 0;
	virtual void OnWasInputError(std::exception_ptr error) noexcept = 0;
};

class SimpleInput final {
	PipeEvent event;

	SimpleInputHandler &handler;

	std::unique_ptr<Buffer> buffer;

public:
	SimpleInput(EventLoop &event_loop, UniqueFileDescriptor pipe,
		    SimpleInputHandler &_handler) noexcept;
	~SimpleInput() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	bool IsActive() const noexcept {
		return buffer != nullptr;
	}

	void Activate() noexcept;

	bool SetLength(std::size_t length) noexcept;

	DisposableBuffer CheckComplete() noexcept;

	/**
	 * Throws on error.
	 */
	void Premature(std::size_t nbytes);

private:
	FileDescriptor GetPipe() const noexcept {
		return event.GetFileDescriptor();
	}

	void OnPipeReady(unsigned events) noexcept;
};

} // namespace Was
