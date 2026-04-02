// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "util/FNVHash.hxx"

#include <cstdint>

namespace RendezvousHashing {

/**
 * The hash algorithm we use for Rendezvous Hashing.  FNV1a is fast
 * and has just the right properties for a good distribution among all
 * nodes.
 *
 * DJB is inferior when the node addresses are too similar (which is
 * often the case when all nodes are on the same local network) and
 * when the sticky_source is too short (e.g. when database serial
 * numbers are used) due to its small prime (33).
 */
using HashAlgorithm = FNV1aAlgorithm<FNVTraits<uint32_t>>;

} // namespace RendezvousHashing
