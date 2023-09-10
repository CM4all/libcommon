// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

class UniqueFileDescriptor;

/**
 * Open the directory "/proc/<pid>" as O_PATH; if pid==0, then it
 * opens "/proc/self".
 *
 * Throws on error.
 */
UniqueFileDescriptor
OpenProcPid(unsigned pid);
