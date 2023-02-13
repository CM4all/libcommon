// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "IniParser.hxx"
#include "FileLineParser.hxx"

#include <cassert>

class IgnoreIniSection final : public IniSectionParser {
public:
	void Property(std::string_view, FileLineParser &) override {}
};

void
IniFileParser::ParseLine(FileLineParser &line)
{
	if (line.SkipSymbol('[')) {
		line.Strip();

		const char *name =
			line.ExpectWordAndSymbol(']',
						 "Section name expected",
						 "']' expected");
		line.ExpectEnd();

		if (child) {
			child->Finish();
			child.reset();
		}

		child = Section(name);
		if (!child)
			child = std::make_unique<IgnoreIniSection>();
	} else if (child) {
		const char *name =
			line.ExpectWordAndSymbol('=',
						 "Property name expected",
						 "'=' expected");
		child->Property(name, line);
	} else {
		throw LineParser::Error{"Section header expected"};
	}
}

void
IniFileParser::Finish()
{
	if (child) {
		child->Finish();
		child.reset();
	}
}
