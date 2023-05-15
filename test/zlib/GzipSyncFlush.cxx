// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/* Compress data from stdin, write to stdout with Z_SYNC_FLUSH */

#include "io/FdReader.hxx"
#include "io/FdOutputStream.hxx"
#include "lib/zlib/GzipOutputStream.hxx"
#include "util/PrintException.hxx"

#include <array>

#include <stdlib.h>
#include <stdio.h>

static void
Copy(OutputStream &os, Reader &r)
{
	while (true) {
		std::array<std::byte, 16384> buffer;
		std::size_t nbytes = r.Read(&buffer, sizeof(buffer));
		if (nbytes == 0)
			break;

		os.Write(std::span{buffer}.first(nbytes));
	}
}

int
main(int, char **) noexcept
try {
	FdReader reader{FileDescriptor{STDIN_FILENO}};
	FdOutputStream fos{FileDescriptor{STDOUT_FILENO}};
	GzipOutputStream gos{fos};

	Copy(gos, reader);

	gos.SyncFlush();
	gos.Finish();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
