// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef DESTRUCT_OBSERVER_HXX
#define DESTRUCT_OBSERVER_HXX

#include "IntrusiveList.hxx"

class DestructAnchor;

/**
 * A class which observes the destruction of a #DestructAnchor
 * instance.
 */
class DestructObserver : public AutoUnlinkIntrusiveListHook {
public:
	explicit DestructObserver(DestructAnchor &anchor) noexcept;

	/**
	 * Was the observed object destructed?
	 */
	operator bool() const noexcept {
		return !is_linked();
	}
};

/**
 * An object which notifies all of its observers about its
 * destruction.
 */
class DestructAnchor {
	friend class DestructObserver;

	IntrusiveList<DestructObserver> observers;

public:
	/**
	 * IntrusiveList's destructor will mark all observers as
	 * "unlinked".
	 */
	~DestructAnchor() noexcept = default;
};

inline
DestructObserver::DestructObserver(DestructAnchor &anchor) noexcept
{
	anchor.observers.push_front(*this);
}

/**
 * An alias for #DestructAnchor which is eliminated from non-debug
 * builds (NDEBUG).
 */
#ifdef NDEBUG
class DebugDestructAnchor {};
#else
using DebugDestructAnchor = DestructAnchor;
#endif

#endif
