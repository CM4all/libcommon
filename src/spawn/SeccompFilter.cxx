// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
		throw FmtErrno(-error, "seccomp_add_arch({}) failed",
			       arch_token);
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
