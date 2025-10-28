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
PipeLineReader::TryRead(bool flush, bool hangup) noexcept
{
	assert(!buffer.IsFull());
	assert(event.IsDefined());

	auto w = buffer.Write();
	assert(!w.empty());

	auto nbytes = event.GetFileDescriptor().Read(std::as_writable_bytes(w));
	if (nbytes <= 0) [[unlikely]] {
		if (nbytes < 0 && errno == EAGAIN)
			return;

		event.Close();
		flush = true;
	}

	if (hangup && static_cast<std::size_t>(nbytes) < w.size()) {
		/* the peer has closed the write end of the pipe and
		   we had a short read: there won't ever be any more
		   data, so let's stop here */
		event.Close();
		flush = true;
	}

	buffer.Append(nbytes);

	while (true) {
		auto r = ExtractLine(buffer, flush);
		if (r.data() == nullptr)
			break;

		if (!handler.OnPipeLine(r))
			return;
	}

	if (!event.IsDefined())
		handler.OnPipeEnd();
}

inline void
PipeLineReader::OnPipeReadable(unsigned events) noexcept
{
	TryRead(false, (events & event.DEAD_MASK) != 0);
}
