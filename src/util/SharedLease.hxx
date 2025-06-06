// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cassert>
#include <utility> // for std::exchange()

/**
 * The object referred to by #SharedLease.  Its OnAbandoned() method
 * will be called each time the last lease gets released.
 *
 * Not thread-safe!
 */
class SharedAnchor {
	friend class SharedLease;

	std::size_t n_leases = 0;

public:
	constexpr SharedAnchor() noexcept = default;

	constexpr ~SharedAnchor() noexcept{
		assert(n_leases == 0);
	}

	SharedAnchor(const SharedAnchor &) = delete;
	SharedAnchor &operator=(const SharedAnchor &) = delete;

	/**
	 * Have all leases been released?  False if at least one
	 * #SharedLease referencing this object remains.
	 */
	constexpr bool IsAbandoned() const noexcept {
		return n_leases == 0;
	}

protected:
	/**
	 * The last lease was released.  This method is allowed to
	 * destruct this object.
	 */
	virtual void OnAbandoned() noexcept = 0;

	/**
	 * An optional method for implementing
	 * SharedLease::SetBroken().  It is only ever called if there
	 * is at least one lease left, i.e. OnAbandoned() will be
	 * called eventually after that.
	 */
	virtual void OnBroken() noexcept {
		assert(!IsAbandoned());
	}

private:
	constexpr void AddLease() noexcept {
		++n_leases;
	}

	void DropLease() noexcept {
		assert(n_leases > 0);

		--n_leases;
		if (IsAbandoned())
			OnAbandoned();
	}
};

/**
 * Holds a lease to a #SharedAnchor instance.  This can be used to
 * track when some object gets abandoned.
 *
 * Not thread-safe!
 */
class SharedLease {
	SharedAnchor *anchor = nullptr;

public:
	/**
	 * Construct an empty lease (one that does not point to any
	 * anchor).
	 */
	constexpr SharedLease() noexcept = default;

	/**
	 * Construct a least pointing to the specified #SharedAnchor.
	 */
	constexpr SharedLease(SharedAnchor &_anchor) noexcept
		:anchor(&_anchor)
	{
		anchor->AddLease();
	}

	constexpr SharedLease(SharedLease &&src) noexcept
		:anchor(std::exchange(src.anchor, nullptr)) {}

	constexpr SharedLease(const SharedLease &src) noexcept
		:anchor(src.anchor)
	{
		if (anchor != nullptr)
			anchor->AddLease();
	}

	~SharedLease() noexcept {
		if (anchor != nullptr)
			anchor->DropLease();
	}

	constexpr SharedLease &operator=(SharedLease &&src) noexcept {
		using std::swap;
		swap(anchor, src.anchor);
		return *this;
	}

	SharedLease &operator=(const SharedLease &src) noexcept {
		if (anchor != nullptr)
			anchor->DropLease();
		anchor = src.anchor;
		if (anchor != nullptr)
			anchor->AddLease();
		return *this;
	}

	/**
	 * Does this object own a least to a #SharedAnchor?
	 */
	explicit constexpr operator bool() const noexcept {
		return anchor != nullptr;
	}

	/**
	 * Obtain a reference to the anchor (which must be valid).
	 * This can be used to cast the reference to a derived type.
	 */
	constexpr SharedAnchor &GetAnchor() const noexcept {
		assert(anchor != nullptr);

		return *anchor;
	}

	/**
	 * Mark the referenced #SharedAnchor as "broken".  Call this
	 * when you see that the resource whose lifetime it is
	 * managing aeppars defunct and should not be used again by
	 * anybody else.
	 *
	 * This requires that the SharedAnchor::OnBroken() method is
	 * implemented.  Calling it without a proper implementation
	 * results in undefined behavior.
	 */
	void SetBroken() const noexcept {
		assert(anchor != nullptr);

		anchor->OnBroken();
	}
};

/**
 * A wrapper for #SharedLease that supports casting the anchor into
 * the specified type.
 */
template<typename T>
class SharedLeasePtr {
	SharedLease lease;

public:
	constexpr SharedLeasePtr() noexcept = default;

	constexpr SharedLeasePtr(T &object) noexcept
		:lease(static_cast<SharedAnchor &>(object))
	{
	}

	constexpr SharedLeasePtr(SharedLeasePtr &&src) noexcept = default;
	constexpr SharedLeasePtr(const SharedLeasePtr &src) noexcept = default;

	~SharedLeasePtr() noexcept = default;

	constexpr SharedLeasePtr &operator=(SharedLeasePtr &&src) noexcept = default;
	SharedLeasePtr &operator=(const SharedLeasePtr &src) noexcept = default;

	constexpr operator bool() const noexcept {
		return static_cast<bool>(lease);
	}

	constexpr T &operator*() const noexcept {
		return static_cast<T &>(lease.GetAnchor());
	}

	constexpr T *operator->() const noexcept {
		return &static_cast<T &>(lease.GetAnchor());
	}
};
