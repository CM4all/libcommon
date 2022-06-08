/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <stdint.h>

struct CgroupState;

struct SystemdUnitProperties {
	/**
	 * CPUWeight; 0 means "undefined" (i.e. use systemd's
	 * default).
	 */
	uint64_t cpu_weight = 0;

	/**
	 * TasksMax; 0 means "undefined" (i.e. use systemd's default).
	 */
	uint64_t tasks_max = 0;

	/**
	 * MemoryMin; 0 means "undefined" (i.e. use systemd's
	 * default).
	 */
	uint64_t memory_min = 0;

	/**
	 * MemoryLow; 0 means "undefined" (i.e. use systemd's
	 * default).
	 */
	uint64_t memory_low = 0;

	/**
	 * MemoryHigh; 0 means "undefined" (i.e. use systemd's
	 * default).
	 */
	uint64_t memory_high = 0;

	/**
	 * MemoryMax; 0 means "undefined" (i.e. use systemd's
	 * default).
	 */
	uint64_t memory_max = 0;

	/**
	 * MemorySwapMax; 0 means "undefined" (i.e. use systemd's
	 * default).
	 */
	uint64_t memory_swap_max = 0;

	/**
	 * IOWeight; 0 means "undefined" (i.e. use systemd's default).
	 */
	uint64_t io_weight = 0;
};

/**
 * Create a new systemd scope and move the current process into it.
 *
 * Throws std::runtime_error on error.
 *
 * @param properties properties to be passed to systemd
 * @param slice create the new scope in this slice (optional)
 */
CgroupState
CreateSystemdScope(const char *name, const char *description,
		   const SystemdUnitProperties &properties,
		   int pid, bool delegate=false, const char *slice=nullptr);
