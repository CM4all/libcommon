// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "system/Arch.hxx"

#include <cstddef>
#include <cstdint>
#include <span>

union InetAddress;

/**
 * Common definitions for our Rendezvous Hashing implementation.  It
 * is used by several of our daemons.  These definitions help keeping
 * all of them in sync, so all of them map to the same physical
 * servers.
 */
namespace RendezvousHashing {

class Node {
	/**
	 * The weight of this node (received in a Zeroconf TXT
	 * record).  We store the negative value because this
	 * eliminates one minus operator from the method
	 * CalculateRendezvousScore().
	 */
	double negative_weight;

	/**
	 * A score for rendezvous hashing calculated from the hash of
	 * the sticky attribute of the current request (e.g. the
	 * "Host" header) and this server address.
	 */
	double rendezvous_score;

	/**
	 * The precalculated hash of #address for Rendezvous
	 * Hashing.
	 */
	uint32_t address_hash;

	Arch arch;

public:
	constexpr Node() noexcept = default;

	explicit constexpr Node(const InetAddress &address,
				Arch _arch, double _weight) noexcept {
		Update(address, _arch, _weight);
	}

	void Update(const InetAddress &address,
		    Arch _arch, double _weight) noexcept;

	[[gnu::pure]]
	double CalculateRendezvousScore(std::span<const std::byte> sticky_source) const noexcept;

	void UpdateRendezvousScore(std::span<const std::byte> sticky_source) noexcept {
		rendezvous_score = CalculateRendezvousScore(sticky_source);
	}

	constexpr Arch GetArch() const noexcept {
		return arch;
	}

	constexpr double GetRendezvousScore() const noexcept {
		return rendezvous_score;
	}

	/**
	 * A function that compares two nodes in a way that makes
	 * std::sort() put the best match first.
	 */
	static constexpr bool Compare(Arch arch, const Node &a, const Node &b) noexcept {
		if (arch != Arch::NONE && a.GetArch() != b.GetArch()) {
			[[unlikely]]

			if (a.GetArch() == arch)
				return true;

			if (b.GetArch() == arch)
				return false;
		}

		return a.GetRendezvousScore() > b.GetRendezvousScore();
	}
};

} // namespace RendezvousHashing
