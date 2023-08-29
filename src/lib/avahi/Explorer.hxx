// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Browser.hxx"
#include "Resolver.hxx"
#include "ConnectionListener.hxx"
#include "net/AllocatedSocketAddress.hxx"

#include <avahi-client/lookup.h>

#include <map>
#include <string>

namespace Avahi {

class ErrorHandler;
class Client;
class ServiceExplorerListener;

/**
 * An explorer for services discovered by Avahi.  It creates a service
 * browser and resolves all objects.  A listener gets notified on each
 * change.
 */
class ServiceExplorer final : ConnectionListener {
	ErrorHandler &error_handler;

	Client &avahi_client;
	ServiceExplorerListener &listener;

	const AvahiIfIndex query_interface;
	const AvahiProtocol query_protocol;
	const std::string query_type;
	const std::string query_domain;

	ServiceBrowserPtr avahi_browser;

	class Object {
		ServiceExplorer &explorer;

		ServiceResolverPtr resolver;

		AllocatedSocketAddress address;

	public:
		explicit Object(ServiceExplorer &_explorer) noexcept
			:explorer(_explorer) {}

		Object(const Object &) = delete;
		Object &operator=(const Object &) = delete;

		const std::string &GetKey() const noexcept;

		bool IsActive() const noexcept {
			return !address.IsNull();
		}

		bool HasFailed() const noexcept {
			return resolver == nullptr && !IsActive();
		}

		void Resolve(AvahiClient *client, AvahiIfIndex interface,
			     AvahiProtocol protocol,
			     const char *name,
			     const char *type,
			     const char *domain) noexcept;
		void CancelResolve() noexcept;

	private:
		void ServiceResolverCallback(AvahiIfIndex interface,
					     AvahiResolverEvent event,
					     const AvahiAddress *a,
					     uint16_t port) noexcept;
		static void ServiceResolverCallback(AvahiServiceResolver *r,
						    AvahiIfIndex interface,
						    AvahiProtocol protocol,
						    AvahiResolverEvent event,
						    const char *name,
						    const char *type,
						    const char *domain,
						    const char *host_name,
						    const AvahiAddress *a,
						    uint16_t port,
						    AvahiStringList *txt,
						    AvahiLookupResultFlags flags,
						    void *userdata) noexcept;
	};

	using Map = std::map<std::string, Object>;
	Map objects;

	std::size_t n_resolvers = 0;

	bool all_for_now_pending = false;

public:
	ServiceExplorer(Client &_avahi_client,
			ServiceExplorerListener &_listener,
			AvahiIfIndex _interface,
			AvahiProtocol _protocol,
			const char *_type,
			const char *_domain,
			ErrorHandler &_error_handler) noexcept;
	~ServiceExplorer() noexcept;

private:
	void ServiceBrowserCallback(AvahiServiceBrowser *b,
				    AvahiIfIndex interface,
				    AvahiProtocol protocol,
				    AvahiBrowserEvent event,
				    const char *name,
				    const char *type,
				    const char *domain,
				    AvahiLookupResultFlags flags) noexcept;
	static void ServiceBrowserCallback(AvahiServiceBrowser *b,
					   AvahiIfIndex interface,
					   AvahiProtocol protocol,
					   AvahiBrowserEvent event,
					   const char *name,
					   const char *type,
					   const char *domain,
					   AvahiLookupResultFlags flags,
					   void *userdata) noexcept;

	/* virtual methods from class AvahiConnectionListener */
	void OnAvahiConnect(AvahiClient *client) noexcept override;
	void OnAvahiDisconnect() noexcept override;
};

} // namespace Avahi
