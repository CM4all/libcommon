/*
 * Copyright 2007-2017 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BENG_PROXY_TRANSLATE_PARSER_HXX
#define BENG_PROXY_TRANSLATE_PARSER_HXX

#include "PReader.hxx"
#include "Response.hxx"
#include "adata/ExpandableStringList.hxx"
#include "AllocatorPtr.hxx"
#include "util/TrivialArray.hxx"

#if TRANSLATION_ENABLE_RADDRESS || TRANSLATION_ENABLE_HTTP || TRANSLATION_ENABLE_WANT || TRANSLATION_ENABLE_RADDRESS
#include "translation/Request.hxx"
#endif

struct FileAddress;
struct CgiAddress;
struct HttpAddress;
struct LhttpAddress;
struct NfsAddress;
struct JailParams;
struct ChildOptions;
struct NamespaceOptions;
struct MountList;
struct AddressList;
struct Transformation;
struct StringView;

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
             want_full_uri(!r.want_full_uri.IsNull()),
#endif
             want(!r.want.empty())
#if TRANSLATION_ENABLE_RADDRESS
            , content_type_lookup(!r.content_type_lookup.IsNull())
#endif
        {}
    } from_request;
#endif

    /**
     * Has #TranslationCommand::BEGIN been seen already?
     */
    bool begun = false;

    TranslatePacketReader reader;
    TranslateResponse response;

    TranslationCommand previous_command;

    TrivialArray<const char *, 16> probe_suffixes_builder;

#if TRANSLATION_ENABLE_RADDRESS
    /** the current resource address being edited */
    ResourceAddress *resource_address;
#endif

    /** the current JailCGI parameters being edited */
    JailParams *jail;

    /** the current child process options being edited */
    ChildOptions *child_options;

    /** the current namespace options being edited */
    NamespaceOptions *ns_options;

    /** the tail of the current mount_list */
    MountList **mount_list;

#if TRANSLATION_ENABLE_RADDRESS
    /** the current local file address being edited */
    FileAddress *file_address;

    /** the current HTTP/AJP address being edited */
    HttpAddress *http_address;

    /** the current CGI/FastCGI/WAS address being edited */
    CgiAddress *cgi_address;

    /** the current NFS address being edited */
    NfsAddress *nfs_address;

    /** the current "local HTTP" address being edited */
    LhttpAddress *lhttp_address;

    /** the current address list being edited */
    AddressList *address_list;
#endif

    ExpandableStringList::Builder env_builder, args_builder;

#if TRANSLATION_ENABLE_RADDRESS
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
    WidgetView **widget_view_tail;
#endif

#if TRANSLATION_ENABLE_TRANSFORMATION
    /** the current transformation */
    Transformation *transformation;

    /** pointer to the tail of the transformation linked list */
    Transformation **transformation_tail;
#endif

public:
    explicit TranslateParser(AllocatorPtr _alloc
#if TRANSLATION_ENABLE_RADDRESS || TRANSLATION_ENABLE_HTTP || TRANSLATION_ENABLE_WANT || TRANSLATION_ENABLE_RADDRESS
                             , const TranslateRequest &r
#endif
                             )
        :alloc(_alloc)
#if TRANSLATION_ENABLE_RADDRESS || TRANSLATION_ENABLE_HTTP || TRANSLATION_ENABLE_WANT || TRANSLATION_ENABLE_RADDRESS
        , from_request(r)
#endif
    {
    }

    size_t Feed(const uint8_t *data, size_t length) {
        return reader.Feed(alloc, data, length);
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
    bool HasArgs() const {
#if TRANSLATION_ENABLE_RADDRESS
        if (cgi_address != nullptr || lhttp_address != nullptr)
            return true;
#endif

#if TRANSLATION_ENABLE_EXECUTE
        if (response.execute != nullptr)
            return true;
#endif

        return false;
    }

    void SetChildOptions(ChildOptions &_child_options);

#if TRANSLATION_ENABLE_RADDRESS
    void SetCgiAddress(ResourceAddress::Type type, const char *path);
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
    template<typename T>
    Transformation *AddTransformation(T type) noexcept;
    ResourceAddress *AddFilter();
    void AddSubstYamlFile(const char *path) noexcept;
#endif

    void HandleBindMount(StringView payload,
                         bool expand, bool writable, bool exec=false);

    void HandleRefence(StringView payload);

#if TRANSLATION_ENABLE_WANT
    void HandleWant(const TranslationCommand *payload, size_t payload_length);
#endif

#if TRANSLATION_ENABLE_RADDRESS
    void HandleContentTypeLookup(ConstBuffer<void> payload);
#endif

    void HandleRegularPacket(TranslationCommand command,
                             ConstBuffer<void> payload);

    void HandleUidGid(ConstBuffer<void> payload);
    void HandleUmask(ConstBuffer<void> payload);

    void HandleCgroupSet(StringView payload);

    void HandleSubstYamlFile(StringView payload);

    Result HandlePacket(TranslationCommand command, ConstBuffer<void> payload);
};

#endif
