// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

void
InitProcessName(int argc, char **argv) noexcept;

void
SetProcessName(const char *name) noexcept;
