// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#ifndef HASH_RING_HXX
#define HASH_RING_HXX

#include <algorithm> // for std::fill()
#include <array>

#include <stddef.h>

class AllocatorPtr;

/**
 * Consistent Hashing implementation.
 *
 * @param Node the node type
 * @param hash_t the type of a hash value
 * @param N_BUCKETS the number of buckets in the ring
 * @param N_REPLICAS the number of replicas in the ring for each node
 *
 * @see https://en.wikipedia.org/wiki/Consistent_hashing
 */
template<typename Node, typename hash_t,
	 size_t N_BUCKETS, size_t N_REPLICAS>
class HashRing {
	std::array<Node *, N_BUCKETS> buckets;

public:
	/**
	 * Build the hash ring using nodes from the given container.
	 *
	 * @param nodes a non-empty iterable container which contains
	 * nodes; pointers to those nodes will be stored in this object
	 * (i.e. they must be valid as long as this object is used)
	 * @param hasher a functor object which generates a secure hash of
	 * a node and a replica number
	 */
	template<typename C, typename H>
	void Build(C &&nodes, H &&hasher) noexcept {
		/* clear all buckets */
		std::fill(buckets.begin(), buckets.end(), nullptr);

		/* inject nodes (and their replicas) at certain buckets */
		for (auto &node : nodes)
			for (size_t replica = 0; replica < N_REPLICAS; ++replica)
				buckets[hasher(node, replica) % N_BUCKETS] = &node;

		/* fill follow-up buckets */
		Node *node = nullptr;
		for (auto &i : buckets) {
			if (i != nullptr)
				/* here, a new node starts */
				node = i;
			else
				/* belongs to the previous node */
				i = node;
		}

		/* handle roll-over */
		for (auto &i : buckets) {
			if (i != nullptr)
				break;
			i = node;
		}
	}

	/**
	 * Pick a node using the given hash.
	 *
	 * Before calling this, Build() must have been called.
	 */
	[[gnu::pure]]
	Node &Pick(hash_t h) const noexcept {
		return *buckets[h % N_BUCKETS];
	}

	/**
	 * Find the next node after the given one.  This is useful for
	 * skipping known-bad nodes and turning to a failover node.
	 *
	 * @return a new hash (for another FindNext() call) and a node
	 * reference (may be equal to the previous node if there is only
	 * one node)
	 */
	[[gnu::pure]]
	std::pair<hash_t, Node &> FindNext(hash_t h) const noexcept {
		auto &node = Pick(h++);

		for (size_t i = buckets.size() - 1;; --i, ++h) {
			auto &n = Pick(h);
			if (i == 0 || &n != &node)
				return {h, n};
		}
	}
};

#endif
