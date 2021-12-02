/*
 * Copyright 2020-2021 CM4all GmbH
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

#include "io/FileDescriptor.hxx"
#include "io/Iovec.hxx"
#include "io/uring/Queue.hxx"
#include "io/uring/Operation.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"
#include "util/StaticFifoBuffer.hxx"

#include <cstdint>
#include <iterator>

#include <stdlib.h>

class ReadWriteOperation final : Uring::Operation {
	Uring::Queue &queue;

	const FileDescriptor read_fd, write_fd;

	off_t read_offset, write_offset;

	struct iovec iov;

	std::exception_ptr error;

	enum class State {
		INIT,
		READ,
		WRITE,
		DONE,
	} state = State::INIT;

	StaticFifoBuffer<uint8_t, 1024> buffer;

public:
	ReadWriteOperation(Uring::Queue &_queue,
			   FileDescriptor _read_fd, FileDescriptor _write_fd,
			   off_t _read_offset, off_t _write_offset) noexcept
		:queue(_queue), read_fd(_read_fd), write_fd(_write_fd),
		 read_offset(_read_offset), write_offset(_write_offset)
	{
		Read();
	}

	bool CheckDone() {
		if (state != State::DONE)
			return false;

		if (error)
			std::rethrow_exception(error);
		return true;
	}

private:
	void Read() {
		state = State::READ;

		auto &s = queue.RequireSubmitEntry();

		ConstBuffer<uint8_t> w = buffer.Write();
		assert(!w.empty());
		iov = MakeIovec(w);
		io_uring_prep_readv(&s, read_fd.Get(), &iov, 1, read_offset);

		queue.Push(s, *this);
	}

	void Write() {
		state = State::WRITE;

		auto *s = queue.GetSubmitEntry();
		assert(s != nullptr);

		ConstBuffer<uint8_t> r = buffer.Read();
		assert(!r.empty());
		iov = MakeIovec(r);
		io_uring_prep_writev(s, write_fd.Get(), &iov, 1, write_offset);

		queue.Push(*s, *this);
	}

	void OnUringCompletion(int res) noexcept override
	try {
		switch (state) {
		case State::INIT:
		case State::DONE:
			assert(false);
			break;

		case State::READ:
			if (res < 0)
				throw MakeErrno(-res, "Failed to read");

			if (res == 0) {
				state = State::DONE;
				return;
			}

			buffer.Append(res);
			read_offset += res;
			Write();
			break;

		case State::WRITE:
			if (res < 0)
				throw MakeErrno(-res, "Failed to write");

			if (res == 0)
				throw std::runtime_error("Short write");

			buffer.Consume(res);
			write_offset += res;
			if (buffer.empty())
				Read();
			else
				Write();
			break;

		}
	} catch (...) {
		error = std::current_exception();
		state = State::DONE;
	}
};

int
main(int, char **) noexcept
try {
	Uring::Queue queue(64, 0);

	ReadWriteOperation operation(queue,
				     FileDescriptor(STDIN_FILENO),
				     FileDescriptor(STDOUT_FILENO),
				     0, 0);

	while (!operation.CheckDone())
		queue.WaitDispatchOneCompletion();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
