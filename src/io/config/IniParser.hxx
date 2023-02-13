// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "ConfigParser.hxx"

#include <memory>

/**
 * Parser properties within an INI section.  Derive from this class
 * and return instances from IniFileParser::Section().
 */
class IniSectionParser {
public:
	virtual ~IniSectionParser() noexcept = default;

	/**
	 * A property was found.
	 *
	 * @param name the name of the property
	 * @param value a #FileLineParser instance which may be fetch
	 * to obtain the value
	 */
	virtual void Property(std::string_view name,
			      FileLineParser &value) = 0;

	/**
	 * Called when the section ends (the file ends or a new
	 * section starts).
	 *
	 * May throw on error (e.g. if the section is incomplete).
	 */
	virtual void Finish() {}
};

/**
 * Parse INI files.  Override Section() and derive section-specific
 * classes from #IniSectionParser.
 */
class IniFileParser : public ConfigParser {
	std::unique_ptr<IniSectionParser> child;

public:
	/**
	 * A section header was found.  This method may decide to
	 * continue parsing the section by returning an
	 * #IniSectionParser instance, ignore the section by returning
	 * nullptr, or throw an error.
	 */
	[[nodiscard]]
	virtual std::unique_ptr<IniSectionParser> Section(std::string_view name) = 0;

	/* virtual methods from ConfigParser */
	virtual void ParseLine(FileLineParser &line) override;
	virtual void Finish() override;
};
