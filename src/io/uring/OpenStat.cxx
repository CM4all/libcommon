// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "OpenStat.hxx"
#include "Close.hxx"
#include "Handler.hxx"
#include "Queue.hxx"

#include <cassert>
#include <cstdint>
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

	auto &s = queue.RequireSubmitEntry();

	io_uring_prep_openat(&s, directory_fd.Get(), path,
			     flags|O_NOCTTY|O_CLOEXEC, mode);
	queue.Push(s, *this);
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

	auto &s = queue.RequireSubmitEntry();

	io_uring_prep_openat2(&s, directory_fd.Get(), path,
			      /* why is this parameter not const? */
			      const_cast<struct open_how *>(&ro_beneath));
	queue.Push(s, *this);
}

void
OpenStat::OnUringCompletion(int res) noexcept
{
	if (canceled) {
		if (!fd.IsDefined() && res >= 0)
			Close(&queue, FileDescriptor{res});

		delete this;
		return;
	}

	if (res < 0) {
		fd.Close();
		handler.OnOpenStatError(-res);
		return;
	}

	if (!fd.IsDefined()) {
		fd = UniqueFileDescriptor(res);

		auto &s = queue.RequireSubmitEntry();

		constexpr unsigned statx_mask =
			STATX_TYPE|STATX_MTIME|STATX_INO|STATX_SIZE;

		io_uring_prep_statx(&s, res, "", AT_EMPTY_PATH,
				    statx_mask, &st);
		queue.Push(s, *this);
	} else {
		handler.OnOpenStat(std::move(fd), st);
	}
}

} // namespace Uring
