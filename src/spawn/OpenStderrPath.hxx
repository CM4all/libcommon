// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

class UniqueFileDescriptor;

UniqueFileDescriptor
OpenStderrPath(const char *path);
