/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "SeccompFilter.hxx"

namespace Seccomp {

Filter::Filter(uint32_t def_action)
	:ctx(seccomp_init(def_action))
{
	if (ctx == nullptr)
		throw std::runtime_error("seccomp_init() failed");
}

void
Filter::Reset(uint32_t def_action)
{
	int error = seccomp_reset(ctx, def_action);
	if (error < 0)
		throw MakeErrno(-error, "seccomp_reset() failed");
}

void
Filter::Load() const
{
	int error = seccomp_load(ctx);
	if (error < 0)
		throw MakeErrno(-error, "seccomp_load() failed");
}

void
Filter::AddArch(uint32_t arch_token)
{
	int error = seccomp_arch_add(ctx, arch_token);
	if (error < 0)
		throw FormatErrno(-error, "seccomp_add_arch(%u) failed",
				  unsigned(arch_token));
}

void
Filter::AddSecondaryArchs() noexcept
{
#if defined(__i386__) || defined(__x86_64__)
	seccomp_arch_add(ctx, SCMP_ARCH_X86);
	seccomp_arch_add(ctx, SCMP_ARCH_X86_64);
	seccomp_arch_add(ctx, SCMP_ARCH_X32);
#endif
}

} // namespace Seccomp
