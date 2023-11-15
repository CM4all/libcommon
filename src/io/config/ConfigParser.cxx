// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ConfigParser.hxx"
#include "FileLineParser.hxx"
#include "io/BufferedReader.hxx"
#include "io/FdReader.hxx"
#include "io/Open.hxx"
#include "io/UniqueFileDescriptor.hxx"
#include "lib/fmt/SystemError.hxx"

#include <algorithm>
#include <exception>
#include <vector>

#include <assert.h>
#include <errno.h>
#include <fnmatch.h>
#include <string.h>

using std::string_view_literals::operator""sv;
namespace fs = std::filesystem;

bool
ConfigParser::PreParseLine(FileLineParser &)
{
	return false;
}

bool
NestedConfigParser::PreParseLine(FileLineParser &line)
{
	if (child) {
		if (child->PreParseLine(line))
			return true;

		if (line.SkipSymbol('}')) {
			line.ExpectEnd();
			child->Finish();
			FinishChild(std::move(child));
			child.reset();
			return true;
		}
	}

	return ConfigParser::PreParseLine(line);
}

void
NestedConfigParser::ParseLine(FileLineParser &line)
{
	if (child)
		child->ParseLine(line);
	else
		ParseLine2(line);
}

void
NestedConfigParser::Finish()
{
	if (child)
		throw LineParser::Error("Block not closed at end of file");

	ConfigParser::Finish();
}

void
NestedConfigParser::SetChild(std::unique_ptr<ConfigParser> &&_child)
{
	assert(!child);

	child = std::move(_child);
}

bool
CommentConfigParser::PreParseLine(FileLineParser &line)
{
	if (child.PreParseLine(line))
		return true;

	if (line.front() == '#' || line.IsEnd())
		/* ignore empty lines and comments */
		return true;

	return ConfigParser::PreParseLine(line);
}

void
CommentConfigParser::ParseLine(FileLineParser &line)
{
	child.ParseLine(line);
}

void
CommentConfigParser::Finish()
{
	child.Finish();
	ConfigParser::Finish();
}

bool
VariableConfigParser::PreParseLine(FileLineParser &line)
{
	return child.PreParseLine(line);
}

void
VariableConfigParser::ParseLine(FileLineParser &line)
{
	Expand(line);

	if (line.SkipWord("@set")) {
		const char *name = line.ExpectWordAndSymbol('=',
							    "Variable name expected",
							    "'=' expected");
		const char *value = line.NextUnescape();
		if (value == nullptr)
			throw LineParser::Error("Quoted value expected after '='");

		line.ExpectEnd();

		auto i = variables.emplace(name, value);
		if (!i.second)
			i.first->second = value;
	} else {
		child.ParseLine(line);
	}
}

void
VariableConfigParser::Finish()
{
	child.Finish();
	ConfigParser::Finish();
}

void
VariableConfigParser::ExpandOne(std::string &dest,
				const char *&src, const char *end) const
{
	assert(src + 2 <= end);
	assert(*src == '$');
	assert(src[1] == '{');

	src += 2;

	if (src >= end || !LineParser::IsWordChar(*src))
		throw LineParser::Error("Variable name expected after '${'");

	const char *name_begin = src++;

	do {
		if (++src >= end)
			throw LineParser::Error("Missing '}' after variable name");
	} while (LineParser::IsWordChar(*src));

	if (*src != '}')
		throw LineParser::Error("Missing '}' after variable name");

	const char *name_end = src++;

	const std::string name(name_begin, name_end);
	auto i = variables.find(name);
	if (i == variables.end())
		throw LineParser::Error{fmt::format("No such variable: {}"sv,
						    name)};

	dest += i->second;
}

void
VariableConfigParser::ExpandQuoted(std::string &dest,
				   const char *src, const char *end) const
{
	while (true) {
		const char *dollar = (const char *)memchr(src, '$', end - src);
		if (dollar == nullptr)
			break;

		dest.append(src, dollar);

		src = dollar;
		ExpandOne(dest, src, end);
	}

	dest.append(src, end);
}

void
VariableConfigParser::Expand(std::string &dest, const char *src) const
{
	while (true) {
		const char ch = *src;
		if (ch == 0)
			break;

		if (ch == '\'') {
			const char *end = strchr(src + 1, '\'');
			if (end == nullptr)
				break;

			++end;
			dest.append(src, end);
			src = end;
		} else if (ch == '"') {
			const char *end = strchr(src + 1, '"');
			if (end == nullptr)
				break;

			dest.push_back(ch);
			ExpandQuoted(dest, src + 1, end);
			dest.push_back(ch);
			src = end + 1;
		} else if (ch == '$' && src[1] == '{') {
			dest.push_back('\'');
			ExpandOne(dest, src, src + strlen(src));
			dest.push_back('\'');
		} else {
			dest.push_back(ch);
			++src;
		}
	}

	dest += src;
}

char *
VariableConfigParser::Expand(const char *src) const
{
	if (strstr(src, "${") == nullptr)
		return nullptr;

	buffer.clear();
	Expand(buffer, src);
	return buffer.data();
}

void
VariableConfigParser::Expand(FileLineParser &line) const
{
	char *p = Expand(line.Rest());
	if (p != nullptr)
		line.Replace(p);
}

bool
IncludeConfigParser::PreParseLine(FileLineParser &line)
{
	return child.PreParseLine(line);
}

void
IncludeConfigParser::ParseLine(FileLineParser &line)
{
	if (line.SkipWord("@include")) {
		auto p = line.ExpectPath();
		line.ExpectEnd();

		IncludePath(std::move(p));
	} else if (line.SkipWord("@include_optional")) {
		auto p = line.ExpectPath();
		line.ExpectEnd();

		IncludeOptionalPath(std::move(p));
	} else
		child.ParseLine(line);
}

void
IncludeConfigParser::Finish()
{
	if (finish_child)
		child.Finish();
}

inline void
IncludeConfigParser::IncludePath(std::filesystem::path &&p)
{
	auto directory = p.parent_path();
	if (directory.empty())
		directory = ".";

	const auto pattern = p.filename();

	if (pattern.native().find('*') != std::string::npos ||
	    pattern.native().find('?') != std::string::npos) {

		std::vector<fs::path> files;

		for (const auto &i : fs::directory_iterator(directory))
			if (fnmatch(pattern.c_str(), i.path().filename().c_str(), 0) == 0)
				files.emplace_back(i.path());

		std::sort(files.begin(), files.end());

		for (auto &i : files) {
			IncludeConfigParser sub(std::move(i), child, false);
			ParseConfigFile(sub.path, sub);
		}
	} else {
		IncludeConfigParser sub(std::move(p), child, false);
		ParseConfigFile(sub.path, sub);
	}
}

static void
ParseConfigFile(const std::filesystem::path &path, BufferedReader &reader,
		ConfigParser &parser)
{
	unsigned i = 1;
	while (char *line = reader.ReadLine()) {
		FileLineParser line_parser(path, line);

		try {
			if (!parser.PreParseLine(line_parser))
				parser.ParseLine(line_parser);
		} catch (...) {
			std::throw_with_nested(LineParser::Error{fmt::format("{}:{}"sv,
									     path.native(), i)});
		}

		++i;
	}
}

inline void
IncludeConfigParser::IncludeOptionalPath(std::filesystem::path &&p)
{
	IncludeConfigParser sub(std::move(p), child, false);

	UniqueFileDescriptor fd;
	if (!fd.OpenReadOnly(sub.path.c_str())) {
		int e = errno;
		switch (e) {
		case ENOENT:
		case ENOTDIR:
			/* silently ignore this error */
			return;

		default:
			throw FmtErrno(e, "Failed to open {}", sub.path.native());
		}
	}

	FdReader fd_reader{fd};
	BufferedReader buffered_reader{fd_reader};

	ParseConfigFile(sub.path, buffered_reader, sub);
	sub.Finish();
}

void
ParseConfigFile(const std::filesystem::path &path, ConfigParser &parser)
{
	const auto fd = OpenReadOnly(path.c_str());
	FdReader fd_reader{fd};
	BufferedReader buffered_reader{fd_reader};

	ParseConfigFile(path, buffered_reader, parser);
	parser.Finish();
}
