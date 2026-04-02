// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Node.hxx"
#include "HashAlgorithm.hxx"
#include "UintToDouble.hxx"
#include "net/InetAddress.hxx"

namespace RendezvousHashing {

void
Node::Update(const InetAddress &address,
	     Arch _arch, double _weight) noexcept
{
	arch = _arch;
	negative_weight = -_weight;
	address_hash = HashAlgorithm::BinaryHash(address.GetSteadyPart());
}

double
Node::CalculateRendezvousScore(std::span<const std::byte> sticky_source) const noexcept
{
	const auto rendezvous_hash = HashAlgorithm::BinaryHash(sticky_source, address_hash);
	return negative_weight / std::log(UintToDouble(rendezvous_hash));
}

} // namespace RendezvousHashing
