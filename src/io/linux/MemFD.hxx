// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

class UniqueFileDescriptor;

UniqueFileDescriptor
CreateMemFD(const char *name, unsigned flags=0);
