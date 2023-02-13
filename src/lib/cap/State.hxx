// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef CAPABILITY_STATE_HXX
#define CAPABILITY_STATE_HXX

#include <span>
#include <utility>

#include <sys/capability.h>

/**
 * OO wrapper for a cap_t.  Requires libcap.
 */
class CapabilityState {
	cap_t value = nullptr;

	explicit CapabilityState(cap_t _value):value(_value) {}

public:
	CapabilityState(const CapabilityState &src)
		:value(src.value != nullptr
		       ? cap_dup(src.value)
		       : nullptr) {}

	CapabilityState(CapabilityState &&src)
		:value(std::exchange(src.value, nullptr)) {}

	~CapabilityState() {
		if (value != nullptr)
			cap_free(value);
	}

	static CapabilityState Empty() {
		return CapabilityState(cap_init());
	}

	/**
	 * Obtain the capability state of the current process.
	 *
	 * Throws std::system_error on error.
	 */
	static CapabilityState Current();

	/**
	 * Parse the given string.
	 *
	 * Throws on error.
	 */
	static CapabilityState FromText(const char *text);

	CapabilityState &operator=(const CapabilityState &src) {
		value = src.value != nullptr ? cap_dup(src.value) : nullptr;
		return *this;
	}

	CapabilityState &operator=(CapabilityState &&src) {
		std::swap(value, src.value);
		return *this;
	}

	void Clear() {
		cap_clear(value);
	}

	void ClearFlag(cap_flag_t flag) {
		cap_clear_flag(value, flag);
	}

	cap_flag_value_t GetFlag(cap_value_t cap, cap_flag_t flag) const;

	void SetFlag(cap_flag_t flag, std::span<const cap_value_t> caps,
		     cap_flag_value_t flag_value);

	/**
	 * Install the capability state represented by this object in
	 * the current process.
	 *
	 * Throws std::system_error on error.
	 */
	void Install();
};

#endif
