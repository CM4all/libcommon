/*
 * Copyright 2016-2022 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef NDEBUG

#include "IntrusiveList.hxx"

/**
 * Derive from this class to verify that its destructor gets called
 * before the process exits.
 */
class LeakDetector {
	friend class LeakDetectorContainer;
	template<auto member> friend struct IntrusiveListMemberHookTraits;

	IntrusiveListHook leak_detector_siblings;

	enum class State {
		INITIAL,
		REGISTERED,
		DESTRUCTED,
	} state = State::INITIAL;

protected:
	LeakDetector() noexcept;
	LeakDetector(const LeakDetector &) noexcept:LeakDetector() {}

	/**
	 * This destructor is virtual only to force RTTI on the
	 * derived class, so we can identify the object type in a
	 * crash dump.
	 */
	virtual ~LeakDetector() noexcept;
};

#else

class LeakDetector {};

#endif
