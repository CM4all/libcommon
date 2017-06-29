/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SYSTEM_PROCESS_NAME_HXX
#define SYSTEM_PROCESS_NAME_HXX

#include <sys/types.h>

void
InitProcessName(int argc, char **argv);

void
SetProcessName(const char *name);

#endif
