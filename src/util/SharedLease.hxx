// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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

	constexpr bool IsAbandoned() const noexcept {
		return n_leases == 0;
	}

protected:
	virtual void OnAbandoned() noexcept = 0;

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
	constexpr SharedLease() noexcept = default;

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
};
