// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "adata/ExpandableStringList.hxx"
#include "spawn/ChildOptions.hxx"

/**
 * Instructions on how to execute a child process.
 */
struct ExecuteOptions {
	const char *shell = nullptr;

	const char *execute = nullptr;

	/**
	 * Command-line arguments for #execute.
	 */
	ExpandableStringList args;

	ChildOptions child_options;

	constexpr ExecuteOptions() noexcept = default;
	ExecuteOptions(ExecuteOptions &&) = default;
	ExecuteOptions &operator=(ExecuteOptions &&) = default;

	constexpr ExecuteOptions(ShallowCopy shallow_copy,
				 const ExecuteOptions &src) noexcept
		:shell(src.shell),
		 execute(src.execute),
		 args(shallow_copy, src.args),
		 child_options(shallow_copy, src.child_options) {}

	[[nodiscard]]
	ExecuteOptions(AllocatorPtr alloc, const ExecuteOptions &src) noexcept;
};
