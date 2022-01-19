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

#include "CapabilityState.hxx"
#include "Error.hxx"

#include <assert.h>

CapabilityState
CapabilityState::Current()
{
	auto value = cap_get_proc();
	if (value == nullptr)
		throw MakeErrno("Failed to read process capabilities");

	return CapabilityState(value);
}

CapabilityState
CapabilityState::FromText(const char *text)
{
	assert(text != nullptr);

	auto value = cap_from_text(text);
	if (value == nullptr)
		throw MakeErrno("Failed to parse capability string");

	return CapabilityState(value);
}

cap_flag_value_t
CapabilityState::GetFlag(cap_value_t cap, cap_flag_t flag) const
{
	assert(value != nullptr);

	cap_flag_value_t flag_value;
	if (cap_get_flag(value, cap, flag, &flag_value) < 0)
		throw MakeErrno("cap_get_flag() failed");

	return flag_value;
}

void
CapabilityState::SetFlag(cap_flag_t flag, std::span<const cap_value_t> caps,
			 cap_flag_value_t flag_value)
{
	assert(value != nullptr);

	if (cap_set_flag(value, flag, caps.size(), caps.data(),
			 flag_value) < 0)
		throw MakeErrno("cap_set_flag() failed");
}

void
CapabilityState::Install()
{
	assert(value != nullptr);

	if (cap_set_proc(value) < 0)
		throw MakeErrno("Failed to install capability state");
}
