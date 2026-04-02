// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Node.hxx"
#include "HashAlgorithm.hxx"
#include "UintToDouble.hxx"
#include "lib/avahi/Arch.hxx"
#include "lib/avahi/Weight.hxx"

namespace RendezvousHashing {

void
Node::Update(const InetAddress &_address,
	     Arch _arch, double _weight) noexcept
{
	address = _address;
	arch = _arch;
	negative_weight = -_weight;
	address_hash = HashAlgorithm::BinaryHash(address.GetSteadyPart());
}

void
Node::Update(const InetAddress &_address,
	     AvahiStringList *txt, Avahi::ObjectFlags _flags) noexcept
{
	Update(_address, Avahi::GetArchFromTxt(txt),
	       Avahi::GetWeightFromTxt(txt));
	flags = _flags;
}

double
Node::CalculateRendezvousScore(std::span<const std::byte> sticky_source) const noexcept
{
	const auto rendezvous_hash = HashAlgorithm::BinaryHash(sticky_source, address_hash);
	return negative_weight / std::log(UintToDouble(rendezvous_hash));
}

} // namespace RendezvousHashing
