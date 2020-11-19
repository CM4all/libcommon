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
#include "Queue.hxx"
#include "system/Error.hxx"
#include "system/KernelVersion.hxx"

#include <cassert>
#include <cstdint>
#include <exception>
#include <utility>

#include <fcntl.h>

static constexpr auto
MakeReadOnlyBeneath() noexcept
{
#ifndef RESOLVE_NO_MAGICLINKS
	constexpr uint64_t RESOLVE_NO_MAGICLINKS = 0x02;
#endif
#ifndef RESOLVE_BENEATH
	constexpr uint64_t RESOLVE_BENEATH = 0x08;
#endif

	struct open_how how{};
	how.flags = O_RDONLY|O_NOCTTY|O_CLOEXEC;
	how.resolve = RESOLVE_BENEATH|RESOLVE_NO_MAGICLINKS;
	return how;
}

static constexpr auto ro_beneath = MakeReadOnlyBeneath();

namespace Uring {

void
OpenStat::StartOpenStat(FileDescriptor directory_fd, const char *path,
			int flags, mode_t mode) noexcept
{
	assert(!fd.IsDefined());

	auto *s = queue.GetSubmitEntry();
	assert(s != nullptr); // TODO: what if the submit queue is full?

	io_uring_prep_openat(s, directory_fd.Get(), path,
			     flags|O_NOCTTY|O_CLOEXEC, mode);
	queue.Push(*s, *this);
}

void
OpenStat::StartOpenStat(const char *path, int flags, mode_t mode) noexcept
{
	StartOpenStat(FileDescriptor(AT_FDCWD), path, flags, mode);
}

void
OpenStat::StartOpenStatReadOnly(FileDescriptor directory_fd,
				const char *path) noexcept
{
	StartOpenStat(directory_fd, path, O_RDONLY);
}

void
OpenStat::StartOpenStatReadOnly(const char *path) noexcept
{
	StartOpenStatReadOnly(FileDescriptor(AT_FDCWD), path);
}

void
OpenStat::StartOpenStatReadOnlyBeneath(FileDescriptor directory_fd,
				       const char *path) noexcept
{
	assert(!fd.IsDefined());

	auto *s = queue.GetSubmitEntry();
	assert(s != nullptr); // TODO: what if the submit queue is full?

	io_uring_prep_openat2(s, directory_fd.Get(), path,
			      /* why is this parameter not const? */
			      const_cast<struct open_how *>(&ro_beneath));
	queue.Push(*s, *this);
}

void
OpenStat::OnUringCompletion(int res) noexcept
try {
	if (canceled) {
		if (!fd.IsDefined() && res >= 0)
			close(res);

		delete this;
		return;
	}

	if (res < 0) {
		fd.Close();
		throw MakeErrno(-res, "Failed to open file");
	}

	if (!fd.IsDefined()) {
		fd = UniqueFileDescriptor(res);

		auto *s = queue.GetSubmitEntry();
		assert(s != nullptr); // TODO: what if the submit queue is full?

		constexpr unsigned statx_mask =
			STATX_TYPE|STATX_MTIME|STATX_INO|STATX_SIZE;

		if (IsKernelVersionOrNewer({5, 6, 11})) {
			io_uring_prep_statx(s, res, "", AT_EMPTY_PATH,
					    statx_mask, &st);
			queue.Push(*s, *this);
		} else {
			/* fall back to plain statx() because OP_STATX
			   is broken on kernel 5.6, fixed in 5.7 by
			   commit
			   5b0bbee4732cbd58aa98213d4c11a366356bba3d
			   (backported to 5.6.11) */
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
