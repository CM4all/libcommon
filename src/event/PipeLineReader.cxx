// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "PipeLineReader.hxx"
#include "util/ExtractLine.hxx"

#include <cerrno>

void
PipeLineReader::TryRead(bool flush) noexcept
{
	assert(!buffer.IsFull());

	auto w = buffer.Write();
	assert(!w.empty());

	auto nbytes = event.GetFileDescriptor().Read(std::as_writable_bytes(w));
	if (nbytes < 0 && errno == EAGAIN)
		return;

	if (nbytes <= 0) {
		event.Close();
		handler.OnPipeEnd();
		return;
	}

	buffer.Append(nbytes);

	while (true) {
		auto r = ExtractLine(buffer, flush);
		if (r.data() == nullptr)
			break;

		if (!handler.OnPipeLine(r))
			return;
	}
}
