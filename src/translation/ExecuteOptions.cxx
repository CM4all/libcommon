// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "translation/Features.hxx"

#if TRANSLATION_ENABLE_EXECUTE
#include "ExecuteOptions.hxx"
#include "AllocatorPtr.hxx"

ExecuteOptions::ExecuteOptions(AllocatorPtr alloc, const ExecuteOptions &src) noexcept
	:shell(alloc.CheckDup(src.shell)),
	 execute(alloc.CheckDup(src.execute)),
	 args(alloc, src.args),
	 child_options(alloc, src.child_options)
{
}

#endif // TRANSLATION_ENABLE_EXECUTE
