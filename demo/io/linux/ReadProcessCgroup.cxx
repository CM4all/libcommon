#include "io/linux/ProcCgroup.hxx"
#include "util/PrintException.hxx"

#include <fmt/format.h>

#include <stdlib.h>

int
main(int argc, char **argv)
try {
	if (argc != 2) {
		fmt::print(stderr, "Usage: {} PID\n", argv[0]);
		return EXIT_FAILURE;
	}

	fmt::print("{}\n", ReadProcessCgroup(atoi(argv[1])));
	return EXIT_SUCCESS;
} catch (const std::exception &e) {
	PrintException(e);
	return EXIT_FAILURE;
}
