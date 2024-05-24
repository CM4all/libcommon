// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "ContentType.hxx"
#include "util/CharUtil.hxx"
#include "util/MimeType.hxx"
#include "util/StringCompare.hxx"

#include <algorithm> // for std::transform()
#include <array>

namespace Net::Log {

using std::string_view_literals::operator""sv;

/**
 * Slow variant of ToString() only to be used at compile time.  It is
 * used to build a table at compile time.
 */
static constexpr std::string_view
_ToString(ContentType content_type) noexcept
{
	switch (content_type) {
	case ContentType::UNSPECIFIED:
	case ContentType::UNKNOWN:
		break;

		// text

	case ContentType::TEXT:
		return "text/*"sv;

	case ContentType::TEXT_CALENDAR:
		return "text/calendar"sv;

	case ContentType::TEXT_CSS:
		return "text/css"sv;

	case ContentType::TEXT_CSV:
		return "text/csv"sv;

	case ContentType::TEXT_HTML:
		return "text/html"sv;

	case ContentType::TEXT_JAVASCRIPT:
		return "text/javascript"sv;

	case ContentType::TEXT_PLAIN:
		return "text/plain"sv;

		// image

	case ContentType::IMAGE:
		return "image/*"sv;

	case ContentType::IMAGE_AVIF:
		return "image/avif"sv;

	case ContentType::IMAGE_BMP:
		return "image/bmp"sv;

	case ContentType::IMAGE_GIF:
		return "image/gif"sv;

	case ContentType::IMAGE_JPEG:
		return "image/jpeg"sv;

	case ContentType::IMAGE_PNG:
		return "image/png"sv;

	case ContentType::IMAGE_SVG_XML:
		return "image/svg+xml"sv;

	case ContentType::IMAGE_TIFF:
		return "image/tiff"sv;

	case ContentType::IMAGE_WEBP:
		return "image/webp"sv;

		// audio

	case ContentType::AUDIO:
		return "audio/*"sv;

	case ContentType::AUDIO_MPEG:
		return "audio/mpeg"sv;

	case ContentType::AUDIO_OGG:
		return "audio/ogg"sv;

	case ContentType::AUDIO_OPUS:
		return "audio/opus"sv;

	case ContentType::AUDIO_WAV:
		return "audio/wav"sv;

	case ContentType::AUDIO_WEBM:
		return "audio/webm"sv;

	case ContentType::VIDEO:
		return "video/*"sv;

	case ContentType::VIDEO_MP4:
		return "video/mp4"sv;

	case ContentType::VIDEO_MPEG:
		return "video/mpeg"sv;

	case ContentType::VIDEO_OGG:
		return "video/ogg"sv;

	case ContentType::VIDEO_WEBM:
		return "video/webm"sv;

	case ContentType::VIDEO_X_MSVIDEO:
		return "video/x-msvideo"sv;

		// font

	case ContentType::FONT:
		return "font/*"sv;

	case ContentType::FONT_TTF:
		return "font/ttf"sv;

	case ContentType::FONT_WOFF:
		return "font/woff"sv;

	case ContentType::FONT_WOFF2:
		return "font/woff2"sv;

		// application

	case ContentType::APPLICATION:
		return "application/*"sv;

	case ContentType::APPLICATION_JSON:
		return "application/json"sv;

	case ContentType::APPLICATION_OCTET_STREAM:
		return "application/octet-stream"sv;

	case ContentType::APPLICATION_PDF:
		return "application/pdf"sv;

	case ContentType::APPLICATION_XML:
		return "application/xml"sv;

	case ContentType::APPLICATION_X_TAR:
		return "application/x-tar"sv;

	case ContentType::APPLICATION_ZIP:
		return "application/zip"sv;
	}

	return {};
}

/**
 * Build a lookup table using _ToString().  This is supposed to be
 * used at compile time to embed the table in ".rodata".
 */
static constexpr auto
BuildContentTypeStrings() noexcept
{
	std::array<std::string_view, 0x100> result;

	for (std::size_t i = 0; i < result.size(); ++i)
		result[i] = _ToString(static_cast<ContentType>(i));

	return result;
}

static constexpr auto content_type_strings = BuildContentTypeStrings();

ContentType
ParseContentType(std::string_view s) noexcept
{
	/* strip the parameters */
	s = GetMimeTypeBase(s);

	/* convert to lower case */
	std::array<char, 32> buffer;
	if (s.size() > buffer.size())
		return ContentType::UNKNOWN;

	std::transform(s.begin(), s.end(), buffer.begin(), ToLowerASCII);
	s = {buffer.data(), s.size()};

	for (std::size_t i = 0; i < content_type_strings.size(); ++i)
		if (s == content_type_strings[i])
			return static_cast<ContentType>(i);

	if (SkipPrefix(s, "text/"sv)) {
		/* translate deprecated strings? */
		if (s == "xml"sv)
			return ContentType::APPLICATION_XML;

		return ContentType::TEXT;
	} else if (s.starts_with("image/"sv)) {
		return ContentType::IMAGE;
	} else if (s.starts_with("audio/"sv)) {
		return ContentType::AUDIO;
	} else if (s.starts_with("video/"sv)) {
		return ContentType::VIDEO;
	} else if (s.starts_with("font/"sv)) {
		return ContentType::FONT;
	} else if (SkipPrefix(s, "application/"sv)) {
		/* drop the "x-" prefix for the translation code
		   below */
		SkipPrefix(s, "x-"sv);

		/* translate deprecated strings? */
		if (s == "javascript"sv)
			return ContentType::TEXT_JAVASCRIPT;
		else if (s == "font-ttf"sv)
			return ContentType::FONT_TTF;
		else if (s == "font-woff"sv)
			return ContentType::FONT_WOFF;
		else if (s == "font-woff2"sv)
			return ContentType::FONT_WOFF2;

		return ContentType::APPLICATION;
	} else
		return ContentType::UNKNOWN;
}

std::string_view
ToString(ContentType content_type) noexcept
{
	return content_type_strings[static_cast<uint8_t>(content_type)];
}

} // namespace Net::Log
