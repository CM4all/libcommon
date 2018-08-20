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

#include "JailConfig.hxx"
#include "AllocatorPtr.hxx"
#include "util/CharUtil.hxx"
#include "util/StringStrip.hxx"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static char *
next_word(char *p)
{
    while (!IsWhitespaceOrNull(*p))
        ++p;

    if (*p == 0)
        return nullptr;

    *p++ = 0;

    p = StripLeft(p);

    if (*p == 0)
        return nullptr;

    return p;
}

bool
JailConfig::Load(const char *path)
{
    assert(root_dir.empty());
    assert(jailed_home.empty());

    FILE *file = fopen(path, "r");
    if (file == nullptr)
        return false;

    char line[4096], *p, *q;
    while ((p = fgets(line, sizeof(line), file)) != nullptr) {
        p = StripLeft(p);

        if (*p == 0 || *p == '#')
            /* ignore comments */
            continue;

        q = next_word(p);
        if (q == nullptr || next_word(q) != nullptr)
            /* silently ignore syntax errors */
            continue;

        if (strcmp(p, "RootDir") == 0)
            root_dir = q;
        else if (strcmp(p, "JailedHome") == 0)
            jailed_home = q;
    }

    fclose(file);
    return !root_dir.empty() && !jailed_home.empty();
}

static const char *
jail_try_translate_path(const char *path,
                        const char *global_prefix, const char *jailed_prefix,
                        AllocatorPtr alloc)
{
    if (jailed_prefix == nullptr)
        return nullptr;

    size_t global_prefix_length = strlen(global_prefix);
    if (memcmp(path, global_prefix, global_prefix_length) != 0)
        return nullptr;

    if (path[global_prefix_length] == '/')
        return alloc.Concat(jailed_prefix, path + global_prefix_length);
    else if (path[global_prefix_length] == 0)
        return jailed_prefix;
    else
        return nullptr;
}

const char *
JailConfig::TranslatePath(const char *path,
                          const char *document_root, AllocatorPtr alloc) const
{
    const char *translated =
        jail_try_translate_path(path, document_root,
                                jailed_home.c_str(), alloc);
    if (translated == nullptr)
        translated = jail_try_translate_path(path, root_dir.c_str(),
                                             "", alloc);
    return translated;
}
