// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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

	IntrusiveListHook<IntrusiveHookMode::NORMAL> leak_detector_siblings;

	enum class State {
		INITIAL,
		REGISTERED,
		DESTRUCTED,
	} state = State::INITIAL;

protected:
	LeakDetector() noexcept;
	LeakDetector(const LeakDetector &) noexcept:LeakDetector() {}

	~LeakDetector() noexcept;

	/**
	 * This is an arbitrary virtual method only to force RTTI on
	 * the derived class, so we can identify the object type in a
	 * crash dump.
	 */
	virtual void Dummy() noexcept;
};

#else

class LeakDetector {};

#endif
