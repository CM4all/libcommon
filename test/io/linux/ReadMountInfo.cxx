#include "io/linux/MountInfo.hxx"
#include "util/PrintException.hxx"

#include <fmt/format.h>

#include <stdlib.h>

int
main(int argc, char **argv)
try {
	if (argc != 3) {
		fmt::print(stderr, "Usage: {} PID MNTPATH\n", argv[0]);
		return EXIT_FAILURE;
	}

	const auto mnt = ReadProcessMount(atoi(argv[1]), argv[2]);
	if (!mnt.IsDefined()) {
		fmt::print(stderr, "Not a mount point: {}\n", argv[2]);
		return EXIT_FAILURE;
	}

	fmt::print("root: {}\n"
		   "filesystem: {}\n"
		   "source: {}\n",
		   mnt.root,
		   mnt.filesystem,
		   mnt.source);

	return EXIT_SUCCESS;
} catch (const std::exception &e) {
	PrintException(e);
	return EXIT_FAILURE;
}
