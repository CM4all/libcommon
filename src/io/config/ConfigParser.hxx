// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>

class FileLineParser;

class ConfigParser {
public:
	virtual ~ConfigParser() noexcept = default;

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

	void SetVariable(std::string name, std::string value);

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
	const std::filesystem::path path;

	ConfigParser &child;

	/**
	 * Does our Finish() override call child.Finish()?  This is a
	 * kludge to avoid calling a foreign child's Finish() method
	 * multiple times, once for each included file.
	 */
	const bool finish_child;

public:
	IncludeConfigParser(std::filesystem::path &&_path,
			    ConfigParser &_child,
			    bool _finish_child=true)
		:path(std::move(_path)), child(_child),
		 finish_child(_finish_child) {}

	/* virtual methods from class ConfigParser */
	bool PreParseLine(FileLineParser &line) override;
	void ParseLine(FileLineParser &line) override;
	void Finish() override;

private:
	void IncludePath(std::filesystem::path &&p);
	void IncludeOptionalPath(std::filesystem::path &&p);
};

struct ShellIncludeParser : public ConfigParser {
	// The child parser has to be a VariableConfigParser, because we want to share a map of variables
	VariableConfigParser &child;

public:
	ShellIncludeParser(VariableConfigParser &_child) : child(_child) {}

	bool PreParseLine(FileLineParser& line) override;
	void ParseLine(FileLineParser &line) override;
	void Finish() override;

private:
	void ParseShellInclude(const std::filesystem::path& path);
};

void
ParseConfigFile(const std::filesystem::path &path, ConfigParser &parser);
