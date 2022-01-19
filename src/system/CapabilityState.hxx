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
