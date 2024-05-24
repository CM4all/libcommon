// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cstdint>
#include <string_view>

namespace Net::Log {

/**
 * Payload of #Attribute::CONTENT_TYPE (the Content-Type response
 * header).
 *
 * This contains only the most common types and omits parameters such
 * as "charset" in order to fit it into one single byte.
 *
 * Important: when adding new codes, make sure that existing integer
 * values remain unchanged, as these are part of the ABI and the
 * protocol.
 *
 * The integer codes are grouped, so all choices of a major type are
 * in the same integer range.  The first choice is a catch-all so (for
 * example) unsupported "image/" types map to "IMAGE" instead of
 * "UNKNOWN".
 */
enum class ContentType : uint8_t {
	/**
	 * No Content-Type at all.  There may be no content at all, or
	 * just no Content-Type specification.  This is equal to this
	 * attribute not being present.
	 */
	UNSPECIFIED = 0,

	/**
	 * There is a Content-Type, but it was not recognized.
	 */
	UNKNOWN,

	TEXT = 0x10,
	TEXT_CALENDAR,
	TEXT_CSS,
	TEXT_CSV,
	TEXT_HTML,
	TEXT_JAVASCRIPT,
	TEXT_PLAIN,

	IMAGE = 0x40,
	IMAGE_AVIF,
	IMAGE_BMP,
	IMAGE_GIF,
	IMAGE_JPEG,
	IMAGE_PNG,
	IMAGE_SVG_XML,
	IMAGE_TIFF,
	IMAGE_WEBP,

	AUDIO = 0x60,
	AUDIO_MPEG,
	AUDIO_OGG,
	AUDIO_OPUS,
	AUDIO_WAV,
	AUDIO_WEBM,

	VIDEO = 0x80,
	VIDEO_MP4,
	VIDEO_MPEG,
	VIDEO_OGG,
	VIDEO_WEBM,
	VIDEO_X_MSVIDEO,

	FONT = 0xa0,
	FONT_TTF,
	FONT_WOFF,
	FONT_WOFF2,

	APPLICATION = 0xc0,
	APPLICATION_JSON,
	APPLICATION_OCTET_STREAM,
	APPLICATION_PDF,
	APPLICATION_XML,
	APPLICATION_X_TAR,
	APPLICATION_ZIP,
};

[[nodiscard]] [[gnu::pure]]
ContentType
ParseContentType(std::string_view s) noexcept;

[[nodiscard]] [[gnu::const]]
std::string_view
ToString(ContentType content_type) noexcept;

} // namespace Net::Log
