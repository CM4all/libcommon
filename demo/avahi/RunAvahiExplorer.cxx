// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "lib/avahi/Check.hxx"
#include "lib/avahi/Client.hxx"
#include "lib/avahi/ErrorHandler.hxx"
#include "lib/avahi/Explorer.hxx"
#include "lib/avahi/ExplorerListener.hxx"
#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "net/ToString.hxx"
#include "util/PrintException.hxx"

class Instance final : Avahi::ServiceExplorerListener, Avahi::ErrorHandler {
	EventLoop event_loop;
	ShutdownListener shutdown_listener;
	Avahi::Client client{event_loop, *this};
	Avahi::ServiceExplorer explorer;

public:
	explicit Instance(const char *service)
		:shutdown_listener(event_loop, BIND_THIS_METHOD(OnShutdown)),
		 explorer(client, *this,
			  AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
			  service, nullptr,
			  *this) {
		shutdown_listener.Enable();
	}

	void Run() noexcept {
		event_loop.Run();
	}

private:
	void OnShutdown() noexcept {
		event_loop.Break();
	}

	/* virtual methods from class Avahi::ServiceExplorerListener */
	void OnAvahiNewObject(const std::string &key,
			      SocketAddress address) noexcept override {
		char buffer[1024];

		printf("new '%s' at %s\n", key.c_str(),
		       ToString(std::span{buffer}, address, "?"));
	}

	void OnAvahiRemoveObject(const std::string &key) noexcept override {
		printf("remove '%s'\n", key.c_str());
	}

	/* virtual methods from class Avahi::ErrorHandler */
	bool OnAvahiError(std::exception_ptr e) noexcept override {
		PrintException(e);
		return true;
	}
};

int
main(int argc, char **argv)
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s SERVICE\n", argv[0]);
		return EXIT_FAILURE;
	}

	const auto service_type = MakeZeroconfServiceType(argv[1], "_tcp");

	Instance instance(service_type.c_str());
	instance.Run();

	return EXIT_SUCCESS;
} catch (const std::exception &e) {
	PrintException(e);
	return EXIT_FAILURE;
}
