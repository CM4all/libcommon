// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "system/linux/listmount.h"
#include "system/linux/statmount.h"
#include "system/Error.hxx"
#include "util/PrintException.hxx"

#include <fmt/core.h>

#include <span>

#include <fcntl.h> // for AT_FDCWD
#include <sys/stat.h> // for statx()

#ifndef STATX_MNT_ID_UNIQUE
#define STATX_MNT_ID_UNIQUE 0x00004000U
#endif

static __u64
GetMountId(const char *path)
{
	struct statx stx;
	if (statx(AT_FDCWD, path, AT_STATX_SYNC_AS_STAT,
		  STATX_MNT_ID_UNIQUE, &stx) < 0)
		throw MakeErrno("statx() failed");

	return stx.stx_mnt_id;
}

static std::span<const __u64>
ListMount(__u64 mnt_id, std::span<__u64> buffer)
{
	const struct mnt_id_req req{
		.size = sizeof(req),
		.mnt_id = mnt_id,
	};

	int result = listmount(&req, buffer.data(), buffer.size(), 0);
	if (result < 0)
		throw MakeErrno("listmount() failed");

	return buffer.first(result);
}

int
main(int argc, char **argv) noexcept
try {
	const char *path = nullptr;
	if (argc > 1)
		path = argv[1];

	__u64 mnt_ids_buffer[256];
	const auto mnt_ids = ListMount(path != nullptr ? GetMountId(path) : LSMT_ROOT, mnt_ids_buffer);

	for (const __u64 mnt_id : mnt_ids) {
		union {
			struct statmount statmount;
			std::byte raw[8192];
		} u;

		const struct mnt_id_req req{
			.size = sizeof(req),
			.mnt_id = mnt_id,
			.param = STATMOUNT_MNT_POINT|STATMOUNT_FS_TYPE,
		};

		if (do_statmount(&req, &u.statmount, sizeof(u), 0) < 0)
			throw MakeErrno("statmount() failed");

		fmt::print("{} type={}\n",
			   reinterpret_cast<const char *>(&u.statmount + 1) + u.statmount.mnt_point,
			   reinterpret_cast<const char *>(&u.statmount + 1) + u.statmount.fs_type);
	}
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
