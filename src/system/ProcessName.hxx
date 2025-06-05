// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

void
InitProcessName(int argc, char **argv) noexcept;

void
SetProcessName(const char *name) noexcept;
