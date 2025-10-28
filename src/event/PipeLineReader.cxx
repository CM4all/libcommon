// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "PipeLineReader.hxx"
#include "util/ExtractLine.hxx"

#include <cerrno>

PipeLineReader::PipeLineReader(EventLoop &event_loop,
			       UniqueFileDescriptor &&fd,
			       PipeLineReaderHandler &_handler) noexcept
	:event(event_loop, BIND_THIS_METHOD(OnPipeReadable),
	       fd.Release()),
	 handler(_handler)
{
	event.ScheduleRead();
}

PipeLineReader::~PipeLineReader() noexcept
{
	event.Close();
}

void
PipeLineReader::TryRead(bool flush) noexcept
{
	assert(!buffer.IsFull());
	assert(event.IsDefined());

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

inline void
PipeLineReader::OnPipeReadable(unsigned) noexcept
{
	TryRead(false);
}
