// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "lib/avahi/Check.hxx"
#include "lib/avahi/Client.hxx"
#include "lib/avahi/ErrorHandler.hxx"
#include "lib/avahi/Explorer.hxx"
#include "lib/avahi/ExplorerListener.hxx"
#include "lib/avahi/StringListCast.hxx"
#include "lib/fmt/SocketAddressFormatter.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "net/rh/Node.hxx"
#include "net/InetAddress.hxx"
#include "util/PrintException.hxx"
#include "util/SpanCast.hxx"

#include <fmt/core.h>

#include <vector>

using std::string_view_literals::operator""sv;

class Instance final : Avahi::ServiceExplorerListener, Avahi::ErrorHandler {
	EventLoop event_loop;
	ShutdownListener shutdown_listener;
	Avahi::Client client{event_loop, *this};
	Avahi::ServiceExplorer explorer;

	struct Node final : RendezvousHashing::Node {
		const std::string host_name;

		explicit Node(const std::string_view _host_name) noexcept
			:host_name(_host_name) {}
	};

	using NodeMap = std::map<std::string, Node, std::less<>>;
	NodeMap nodes;

	std::exception_ptr error;

public:
	explicit Instance(const char *service)
		:shutdown_listener(event_loop, BIND_THIS_METHOD(OnShutdown)),
		 explorer(client, *this,
			  AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
			  service, nullptr,
			  *this) {
		shutdown_listener.Enable();
	}

	void Run() {
		event_loop.Run();

		if (error)
			std::rethrow_exception(std::move(error));
	}

	void Print(Arch arch, std::span<const std::byte> sticky_source);

private:
	void OnShutdown() noexcept {
		event_loop.Break();
	}

	/* virtual methods from class Avahi::ServiceExplorerListener */
	void OnAvahiNewObject(const std::string &key,
			      const char *host_name,
			      const InetAddress &address,
			      AvahiStringList *txt,
			      [[maybe_unused]] Avahi::ObjectFlags flags) noexcept override {
		auto [it, inserted] = nodes.try_emplace(key, host_name);
		it->second.Update(address, txt, flags);
	}

	void OnAvahiRemoveObject(const std::string &key) noexcept override {
		nodes.erase(key);
	}

	void OnAvahiAllForNow() noexcept override {
		event_loop.Break();
	}

	/* virtual methods from class Avahi::ErrorHandler */
	bool OnAvahiError(std::exception_ptr e) noexcept override {
		error = std::move(e);
		event_loop.Break();
		return true;
	}
};

inline void
Instance::Print(Arch arch, std::span<const std::byte> sticky_source)
{
	std::vector<NodeMap::const_iterator> v;
	for (auto i = nodes.begin(); i != nodes.end(); ++i) {
		i->second.UpdateRendezvousScore(sticky_source);
		v.push_back(i);
	}

	std::sort(v.begin(), v.end(), [arch](NodeMap::const_iterator &a, NodeMap::const_iterator &b){
		return RendezvousHashing::Node::Compare(arch, a->second, b->second);
	});

	for (const NodeMap::const_iterator i : v) {
		const auto &node = i->second;

		fmt::print("{:7.3f} {} {}\n"sv,
			   node.GetRendezvousScore(),
			   node.GetAddress(), node.host_name);
	}
}

int
main(int argc, char **argv)
try {
	if (argc != 3) {
		fmt::print(stderr, "Usage: {} SERVICE STICKY_SOURCE\n", argv[0]);
		return EXIT_FAILURE;
	}

	const auto service_type = MakeZeroconfServiceType(argv[1], "_tcp");
	const std::string_view sticky_source = argv[2];

	Instance instance(service_type.c_str());
	instance.Run();
	instance.Print(Arch::NONE, AsBytes(sticky_source));

	return EXIT_SUCCESS;
} catch (const std::exception &e) {
	PrintException(e);
	return EXIT_FAILURE;
}
