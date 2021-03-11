/*
 * Copyright 2007-2021 CM4all GmbH
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

#include "io/ConfigParser.hxx"
#include "io/FileLineParser.hxx"
#include "util/ScopeExit.hxx"

#include <gtest/gtest.h>

#include <string.h>
#include <stdlib.h>

class MyConfigParser final
    : public ConfigParser, public std::vector<std::string> {
public:
    void ParseLine(FileLineParser &line) override {
        const char *value = line.NextUnescape();
        if (value == nullptr)
            throw LineParser::Error("Quoted value expected");
        line.ExpectEnd();
        emplace_back(value);
    }
};

static void
ParseConfigFile(ConfigParser &parser, const char *const*lines)
{
    while (*lines != nullptr) {
        char *line = strdup(*lines++);
        AtScopeExit(line) { free(line); };

        FileLineParser line_parser({}, line);
        if (!parser.PreParseLine(line_parser))
            parser.ParseLine(line_parser);
    }

    parser.Finish();
}

static const char *const v_data[] = {
    "@set foo='bar'",
    "@set bar=\"${foo}\"",
    "${foo} ",
    "'${foo}'",
    "\"${foo}\"",
    "\"${bar}\"",
    " \"a${foo}b\" ",
    "@set foo=\"with space\"",
    "\"${foo}\"",
    "  ${foo}  ",
    nullptr
};

static const char *const v_output[] = {
    "bar",
    "${foo}",
    "bar",
    "bar",
    "abarb",
    "with space",
    "with space",
    nullptr
};

TEST(ConfigParserTest, VariableConfigParser)
{
    MyConfigParser p;
    VariableConfigParser v(p);

    ParseConfigFile(v, v_data);

    for (size_t i = 0; v_output[i] != nullptr; ++i) {
        ASSERT_LT(i, p.size());
        ASSERT_STREQ(v_output[i], p[i].c_str());
    }
}
