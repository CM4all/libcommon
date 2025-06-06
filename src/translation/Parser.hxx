// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "PReader.hxx"
#include "adata/ExpandableStringList.hxx"
#include "AllocatorPtr.hxx"
#include "util/IntrusiveForwardList.hxx"
#include "util/TrivialArray.hxx"

#if TRANSLATION_ENABLE_RADDRESS
#include "ResourceAddress.hxx"
#include "cluster/AddressListBuilder.hxx"
#include <memory>
#include <vector>
#endif

#if TRANSLATION_ENABLE_WIDGET
#include "widget/VList.hxx"
#endif

#if TRANSLATION_ENABLE_RADDRESS || TRANSLATION_ENABLE_HTTP || TRANSLATION_ENABLE_WANT || TRANSLATION_ENABLE_RADDRESS
#include "translation/Request.hxx"
#endif

#include <string_view>

struct TranslateResponse;
struct TranslateTokenBucketParams;
struct FileAddress;
struct CgiAddress;
struct HttpAddress;
struct LhttpAddress;
struct ExecuteOptions;
struct ChildOptions;
struct NamespaceOptions;
struct MountNamespaceOptions;
struct Mount;
struct AddressList;
struct Transformation;
struct FilterTransformation;
struct TranslationLayoutItem;

/**
 * Parse translation response packets.
 */
class TranslateParser {
	AllocatorPtr alloc;

#if TRANSLATION_ENABLE_RADDRESS || TRANSLATION_ENABLE_HTTP || TRANSLATION_ENABLE_WANT || TRANSLATION_ENABLE_RADDRESS
	struct FromRequest {
#if TRANSLATION_ENABLE_RADDRESS
		const char *uri;
#endif

#if TRANSLATION_ENABLE_HTTP
		bool want_full_uri;
		bool chain;
#endif

		bool want;

#if TRANSLATION_ENABLE_RADDRESS
		bool content_type_lookup;
#endif

		explicit FromRequest(const TranslateRequest &r)
			:
#if TRANSLATION_ENABLE_RADDRESS
			uri(r.uri),
#endif
#if TRANSLATION_ENABLE_HTTP
			want_full_uri(r.want_full_uri.data() != nullptr),
			chain(r.chain.data() != nullptr),
#endif
			want(!r.want.empty())
#if TRANSLATION_ENABLE_RADDRESS
			, content_type_lookup(r.content_type_lookup.data() != nullptr)
#endif
		{}
	} from_request;
#endif

	/**
	 * Has #TranslationCommand::BEGIN been seen already?
	 */
	bool begun = false;

	TranslatePacketReader reader;
	TranslateResponse &response;

	TranslationCommand previous_command;

	TrivialArray<const char *, 16> probe_suffixes_builder;

#if TRANSLATION_ENABLE_EXECUTE
	ExecuteOptions *execute_options;
#endif

#if TRANSLATION_ENABLE_SPAWN
	/** the current child process options being edited */
	ChildOptions *child_options;

	/** the tail of the current mount_list */
	IntrusiveForwardList<Mount>::iterator mount_list;

	ExpandableStringList::Builder env_builder, args_builder;
#endif

#if TRANSLATION_ENABLE_RADDRESS
	const char *base_suffix = nullptr;

	std::shared_ptr<std::vector<TranslationLayoutItem>> layout_items_builder;

	/** the current resource address being edited */
	ResourceAddress *resource_address;

	/** the current local file address being edited */
	FileAddress *file_address;

	/** the current HTTP/AJP address being edited */
	HttpAddress *http_address;

	/** the current CGI/FastCGI/WAS address being edited */
	CgiAddress *cgi_address;

	/** the current "local HTTP" address being edited */
	LhttpAddress *lhttp_address;

	/** the current address list being edited */
	AddressList *address_list;

	AddressListBuilder address_list_builder;

	ExpandableStringList::Builder params_builder;

	/**
	 * Default port for #TranslationCommand::ADDRESS_STRING.
	 */
	int default_port;
#endif

#if TRANSLATION_ENABLE_WIDGET
	/** the current widget view */
	WidgetView *view;

	/** pointer to the tail of the transformation view linked list */
	WidgetViewList::iterator widget_view_tail;
#endif

#if TRANSLATION_ENABLE_TRANSFORMATION
	/** the current transformation */
	Transformation *transformation;

	/** pointer to the tail of the transformation linked list */
	IntrusiveForwardList<Transformation>::iterator transformation_tail;

	FilterTransformation *filter;
#endif

public:
	explicit TranslateParser(AllocatorPtr _alloc,
#if TRANSLATION_ENABLE_RADDRESS || TRANSLATION_ENABLE_HTTP || TRANSLATION_ENABLE_WANT || TRANSLATION_ENABLE_RADDRESS
				 const TranslateRequest &r,
#endif
				 TranslateResponse &_response)
		:alloc(_alloc),
#if TRANSLATION_ENABLE_RADDRESS || TRANSLATION_ENABLE_HTTP || TRANSLATION_ENABLE_WANT || TRANSLATION_ENABLE_RADDRESS
		 from_request(r),
#endif
		 response(_response)
	{
	}

	std::size_t Feed(std::span<const std::byte> src) noexcept {
		return reader.Feed(alloc, src);
	}

	enum class Result {
		MORE,
		DONE,
	};

	/**
	 * Throws std::runtime_error on error.
	 */
	Result Process();

	TranslateResponse &GetResponse() {
		return response;
	}

private:
#if TRANSLATION_ENABLE_SPAWN
	bool HasArgs() const noexcept;

	void SetChildOptions(ChildOptions &_child_options) noexcept;

	[[gnu::pure]]
	NamespaceOptions *GetNamespaceOptions() noexcept;

#if TRANSLATION_ENABLE_EXECUTE
	const ExecuteOptions *GetExecuteOptions() const noexcept;
	ExecuteOptions &MakeExecuteOptions(const char *error_message);
#endif

	ChildOptions &MakeChildOptions(const char *error_message);
	ExpandableStringList::Builder &MakeEnvBuilder(const char *error_message);
	NamespaceOptions &MakeNamespaceOptions(const char *error_message);
	MountNamespaceOptions &MakeMountNamespaceOptions(const char *error_message);
	void AddMount(const char *error_message, Mount *mount);
#endif // TRANSLATION_ENABLE_SPAWN

#if TRANSLATION_ENABLE_RADDRESS
	void SetCgiAddress(ResourceAddress::Type type, const char *path);

	void FinishAddressList() noexcept;
#endif

#if TRANSLATION_ENABLE_WIDGET
	/**
	 * Throws std::runtime_error on error.
	 */
	void AddView(const char *name);

	/**
	 * Finish the settings in the current view, i.e. copy attributes
	 * from the "parent" view.
	 *
	 * Throws std::runtime_error on error.
	 */
	void FinishView();
#endif

#if TRANSLATION_ENABLE_TRANSFORMATION
	template<typename... Args>
	Transformation *AddTransformation(Args&&... args) noexcept;
	ResourceAddress *AddFilter();
#endif

#if TRANSLATION_ENABLE_SPAWN
	void HandleMountTmpfs(std::string_view payload, bool writable);

	void HandleMountNamedTmpfs(std::string_view payload);

	void HandlePivotRoot(std::string_view payload);
	void HandleMountRootTmpfs(std::string_view payload);
	void HandleHome(std::string_view payload);
#if TRANSLATION_ENABLE_EXPAND
	void HandleExpandHome(std::string_view payload);
#endif
	void HandleMountHome(std::string_view payload);
	void HandleMountProc(std::string_view payload);
	void HandleMountTmpTmpfs(std::string_view payload);

	void HandleBindMount(std::string_view payload,
			     bool expand, bool writable, bool exec=false,
			     bool file=false);

	void HandleSymlink(std::string_view payload);

	void HandleWriteFile(std::string_view payload);
	void HandleUtsNamespace(std::string_view payload);

	void HandleRlimits(std::string_view payload);
	void HandleStderrPath(std::string_view payload, bool jailed);
	void HandleExpandStderrPath(std::string_view payload);
#endif // TRANSLATION_ENABLE_SPAWN

#if TRANSLATION_ENABLE_WANT
	void HandleWant(const TranslationCommand *payload,
			size_t payload_length);
#endif

#if TRANSLATION_ENABLE_RADDRESS
	void HandleContentTypeLookup(std::span<const std::byte> payload);
#endif

	void HandleRegularPacket(TranslationCommand command,
				 std::span<const std::byte> payload);

#if TRANSLATION_ENABLE_SPAWN
	void HandleUidGid(std::span<const std::byte> payload);
	void HandleRealUidGid(std::span<const std::byte> payload);
	void HandleMappedUidGid(std::span<const std::byte> payload);
	void HandleMappedRealUidGid(std::span<const std::byte> payload);
	void HandleUmask(std::span<const std::byte> payload);

	void HandleCgroupSet(std::string_view payload);
	void HandleCgroupXattr(std::string_view payload);
#endif // TRANSLATION_ENABLE_SPAWN

	void HandleMountListenStream(std::span<const std::byte> payload);

#if TRANSLATION_ENABLE_HTTP
	void HandleAllowRemoteNetwork(std::span<const std::byte> payload);
	void HandleTokenBucketParams(TranslateTokenBucketParams &params,
				     const char *packet_name,
				     std::span<const std::byte> payload);
#endif

	Result HandlePacket(TranslationCommand command,
			    std::span<const std::byte> payload);
};
