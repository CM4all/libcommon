/*
 * Copyright 2007-2019 Content Management AG
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

#ifndef CONFIG_PARSER_HXX
#define CONFIG_PARSER_HXX

#include <boost/filesystem/path.hpp>

#include <map>
#include <memory>
#include <string>
#include <utility>

class FileLineParser;

class ConfigParser {
public:
	virtual ~ConfigParser() {}

	virtual bool PreParseLine(FileLineParser &line);
	virtual void ParseLine(FileLineParser &line) = 0;
	virtual void Finish() {}
};

/**
 * A #ConfigParser which can dynamically forward method calls to a
 * nested #ConfigParser instance.
 */
class NestedConfigParser : public ConfigParser {
	std::unique_ptr<ConfigParser> child;

public:
	/* virtual methods from class ConfigParser */
	bool PreParseLine(FileLineParser &line) override;
	void ParseLine(FileLineParser &line) final;
	void Finish() override;

protected:
	void SetChild(std::unique_ptr<ConfigParser> &&_child);
	virtual void ParseLine2(FileLineParser &line) = 0;

	/**
	 * This virtual method gets called after the given child
	 * parser has finished, before it gets destructed.  This
	 * method gets the chance to do additional checks or take over
	 * ownership.
	 */
	virtual void FinishChild(std::unique_ptr<ConfigParser> &&) {}
};

/**
 * A #ConfigParser which ignores lines starting with '#'.
 */
class CommentConfigParser final : public ConfigParser {
	ConfigParser &child;

public:
	explicit CommentConfigParser(ConfigParser &_child)
		:child(_child) {}

	/* virtual methods from class ConfigParser */
	bool PreParseLine(FileLineParser &line) override;
	void ParseLine(FileLineParser &line) final;
	void Finish() override;
};

/**
 * A #ConfigParser which can define and use variables.
 */
class VariableConfigParser final : public ConfigParser {
	ConfigParser &child;

	std::map<std::string, std::string> variables;

	mutable std::string buffer;

public:
	explicit VariableConfigParser(ConfigParser &_child)
		:child(_child) {}

	/* virtual methods from class ConfigParser */
	bool PreParseLine(FileLineParser &line) override;
	void ParseLine(FileLineParser &line) final;
	void Finish() override;

private:
	void ExpandOne(std::string &dest,
		       const char *&src, const char *end) const;
	void ExpandQuoted(std::string &dest,
			  const char *src, const char *end) const;
	void Expand(std::string &dest, const char *src) const;
	char *Expand(const char *src) const;
	void Expand(FileLineParser &line) const;
};

/**
 * A #ConfigParser which can "include" other files.
 */
class IncludeConfigParser final : public ConfigParser {
	const boost::filesystem::path path;

	ConfigParser &child;

	/**
	 * Does our Finish() override call child.Finish()?  This is a
	 * kludge to avoid calling a foreign child's Finish() method
	 * multiple times, once for each included file.
	 */
	const bool finish_child;

public:
	IncludeConfigParser(boost::filesystem::path &&_path,
			    ConfigParser &_child,
			    bool _finish_child=true)
		:path(std::move(_path)), child(_child),
		 finish_child(_finish_child) {}

	/* virtual methods from class ConfigParser */
	bool PreParseLine(FileLineParser &line) override;
	void ParseLine(FileLineParser &line) override;
	void Finish() override;

private:
	void IncludePath(boost::filesystem::path &&p);
	void IncludeOptionalPath(boost::filesystem::path &&p);
};

void
ParseConfigFile(const boost::filesystem::path &path, ConfigParser &parser);

#endif
