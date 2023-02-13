// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef LINUX_FILE_DESCRIPTOR_HXX
#define LINUX_FILE_DESCRIPTOR_HXX

#include <signal.h>

class UniqueFileDescriptor;

UniqueFileDescriptor
CreateEventFD(unsigned initval=0);

UniqueFileDescriptor
CreateSignalFD(const sigset_t &mask, bool nonblock=true);

UniqueFileDescriptor
CreateMemFD(const char *name, unsigned flags=0);

#endif
