/*
 * author: Max Kellermann <mk@cm4all.com>
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
CapabilityState::SetFlag(cap_flag_t flag, ConstBuffer<cap_value_t> caps,
			 cap_flag_value_t flag_value)
{
	assert(value != nullptr);

	if (cap_set_flag(value, flag, caps.size, caps.data, flag_value) < 0)
		throw MakeErrno("cap_set_flag() failed");
}

void
CapabilityState::Install()
{
	assert(value != nullptr);

	if (cap_set_proc(value) < 0)
		throw MakeErrno("Failed to install capability state");
}
