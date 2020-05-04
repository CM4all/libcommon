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

#include "OpenStat.hxx"
#include "Handler.hxx"
#include "Manager.hxx"
#include "system/Error.hxx"
#include "system/KernelVersion.hxx"
#include "util/PrintException.hxx"

#include <fcntl.h>

namespace Uring {

void
OpenStat::StartOpenStatReadOnly(FileDescriptor directory_fd,
				const char *path) noexcept
{
	assert(!fd.IsDefined());

	auto *s = manager.GetSubmitEntry();
	assert(s != nullptr); // TODO: what if the submit queue is full?

	io_uring_prep_openat(s, directory_fd.Get(), path,
			     O_RDONLY|O_NOCTTY|O_CLOEXEC, 0);
	manager.AddPending(*s, *this);
}

void
OpenStat::StartOpenStatReadOnly(const char *path) noexcept
{
	StartOpenStatReadOnly(FileDescriptor(AT_FDCWD), path);
}

void
OpenStat::OnUringCompletion(int res) noexcept
try {
	if (res < 0) {
		fd.Close();
		throw MakeErrno(-res, "Failed to open file");
	}

	if (!fd.IsDefined()) {
		fd = UniqueFileDescriptor(res);

		auto *s = manager.GetSubmitEntry();
		assert(s != nullptr); // TODO: what if the submit queue is full?

		constexpr unsigned statx_mask =
			STATX_TYPE|STATX_MTIME|STATX_INO|STATX_SIZE;

		if (IsKernelVersionOrNewer({5, 7})) {
			io_uring_prep_statx(s, res, "", AT_EMPTY_PATH,
					    statx_mask, &st);
			manager.AddPending(*s, *this);
		} else {
			/* fall back to plain statx() because OP_STATX
			   is broken on kernel 5.6, fixed in 5.7 by
			   commit
			   5b0bbee4732cbd58aa98213d4c11a366356bba3d
			   (no backport as of 5.6.10) */
			if (statx(res, "", AT_EMPTY_PATH, statx_mask, &st) < 0)
				throw MakeErrno("Failed to access file");

			handler.OnOpenStat(std::move(fd), st);
		}
	} else {
		handler.OnOpenStat(std::move(fd), st);
	}
} catch (...) {
	handler.OnOpenStatError(std::current_exception());
}

} // namespace Uring
