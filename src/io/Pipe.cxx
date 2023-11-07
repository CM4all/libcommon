// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Pipe.hxx"
#include "system/Error.hxx"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <fcntl.h>

#ifdef __linux__

static inline std::pair<UniqueFileDescriptor, UniqueFileDescriptor>
CreatePipe(int flags)
{
	int p[2];
	if (pipe2(p, flags))
		throw MakeErrno("pipe2() failed");

	return {
		UniqueFileDescriptor{p[0]},
		UniqueFileDescriptor{p[1]},
	};
}

#else // __linux__

static inline std::pair<UniqueFileDescriptor, UniqueFileDescriptor>
_CreatePipe()
{
	int p[2];

#ifdef _WIN32
	const int result = _pipe(p, 512, _O_BINARY);
#else
	const int result = pipe(p);
#endif
	if (result)
		throw MakeErrno("pipe() failed");

	return {
		UniqueFileDescriptor{p[0]},
		UniqueFileDescriptor{p[1]},
	};
}

#endif // !__linux__

std::pair<UniqueFileDescriptor, UniqueFileDescriptor>
CreatePipe()
{
#ifdef __linux__
	return CreatePipe(O_CLOEXEC);
#else
	return _CreatePipe();
#endif
}

std::pair<UniqueFileDescriptor, UniqueFileDescriptor>
CreatePipeNonBlock()
{
#ifdef __linux__
	return CreatePipe(O_CLOEXEC|O_NONBLOCK);
#else
	auto p = _CreatePipe();
	p.first.SetNonBlocking();
	p.second.SetNonBlocking();
	return p;
#endif
}
