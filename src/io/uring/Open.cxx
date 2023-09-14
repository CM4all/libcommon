// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Open.hxx"
#include "Close.hxx"
#include "Handler.hxx"
#include "Queue.hxx"
#include "io/FileAt.hxx"
#include "io/UniqueFileDescriptor.hxx"

#include <cstdint>

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
Open::StartOpen(FileAt file, int flags, mode_t mode) noexcept
{
	auto &s = queue.RequireSubmitEntry();

	io_uring_prep_openat(&s, file.directory.Get(), file.name,
			     flags|O_NOCTTY|O_CLOEXEC, mode);
	queue.Push(s, *this);
}

void
Open::StartOpen(const char *path, int flags, mode_t mode) noexcept
{
	StartOpen({FileDescriptor(AT_FDCWD), path}, flags, mode);
}

void
Open::StartOpenReadOnly(FileAt file) noexcept
{
	StartOpen(file, O_RDONLY);
}

void
Open::StartOpenReadOnly(const char *path) noexcept
{
	StartOpenReadOnly({FileDescriptor(AT_FDCWD), path});
}

void
Open::StartOpenReadOnlyBeneath(FileAt file) noexcept
{
	auto &s = queue.RequireSubmitEntry();

	io_uring_prep_openat2(&s, file.directory.Get(), file.name,
			      /* why is this parameter not const? */
			      const_cast<struct open_how *>(&ro_beneath));
	queue.Push(s, *this);
}

void
Open::OnUringCompletion(int res) noexcept
{
	if (canceled) {
		if (res >= 0)
			Close(&queue, FileDescriptor{res});

		delete this;
		return;
	}

	if (res >= 0)
		handler.OnOpen(UniqueFileDescriptor{res});
	else
		handler.OnOpenError(-res);
}

} // namespace Uring
