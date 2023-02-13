// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "BufferedOutputStream.hxx"
#include "OutputStream.hxx"

#include <cstdarg>

#include <string.h>
#include <stdio.h>

#ifdef _UNICODE
#include "system/Error.hxx"
#include <stringapiset.h>
#endif

bool
BufferedOutputStream::AppendToBuffer(const void *data, std::size_t size) noexcept
{
	auto r = buffer.Write();
	if (r.size() < size)
		return false;

	memcpy(r.data(), data, size);
	buffer.Append(size);
	return true;
}

void
BufferedOutputStream::Write(const void *data, std::size_t size)
{
	/* try to append to the current buffer */
	if (AppendToBuffer(data, size))
		return;

	/* not enough room in the buffer - flush it */
	Flush();

	/* see if there's now enough room */
	if (AppendToBuffer(data, size))
		return;

	/* too large for the buffer: direct write */
	os.Write(data, size);
}

void
BufferedOutputStream::Write(const char *p)
{
	Write(p, strlen(p));
}

void
BufferedOutputStream::Format(const char *fmt, ...)
{
	auto r = buffer.Write();
	if (r.empty()) {
		Flush();
		r = buffer.Write();
	}

	/* format into the buffer */
	std::va_list ap;
	va_start(ap, fmt);
	std::size_t size = vsnprintf((char *)r.data(), r.size(), fmt, ap);
	va_end(ap);

	if (gcc_unlikely(size >= r.size())) {
		/* buffer was not large enough; flush it and try
		   again */

		Flush();

		r = buffer.Write();

		if (gcc_unlikely(size >= r.size())) {
			/* still not enough space: grow the buffer and
			   try again */
			++size;
			r = {buffer.Write(size), size};
		}

		/* format into the new buffer */
		va_start(ap, fmt);
		size = vsnprintf((char *)r.data(), r.size(), fmt, ap);
		va_end(ap);

		/* this time, it must fit */
		assert(size < r.size());
	}

	buffer.Append(size);
}

#ifdef _UNICODE

void
BufferedOutputStream::Write(const wchar_t *p)
{
	WriteWideToUTF8(p, wcslen(p));
}

void
BufferedOutputStream::WriteWideToUTF8(const wchar_t *src,
				      std::size_t src_length)
{
	if (src_length == 0)
		return;

	auto r = buffer.Write();
	if (r.empty()) {
		Flush();
		r = buffer.Write();
	}

	int length = WideCharToMultiByte(CP_UTF8, 0, src, src_length,
					 (char *)r.data(), r.size(),
					 nullptr, nullptr);
	if (length <= 0) {
		const auto error = GetLastError();
		if (error != ERROR_INSUFFICIENT_BUFFER)
			throw MakeLastError(error, "UTF-8 conversion failed");

		/* how much buffer do we need? */
		length = WideCharToMultiByte(CP_UTF8, 0, src, src_length,
					     nullptr, 0, nullptr, nullptr);
		if (length <= 0)
			throw MakeLastError(error, "UTF-8 conversion failed");

		/* grow the buffer and try again */
		length = WideCharToMultiByte(CP_UTF8, 0, src, src_length,
					     (char *)buffer.Write(length), length,
					     nullptr, nullptr);
		if (length <= 0)
			throw MakeLastError(error, "UTF-8 conversion failed");
	}

	buffer.Append(length);
}

#endif

void
BufferedOutputStream::Flush()
{
	auto r = buffer.Read();
	if (r.empty())
		return;

	os.Write(r.data(), r.size());
	buffer.Consume(r.size());
}
