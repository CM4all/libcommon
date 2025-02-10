// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Control.hxx"
#include "net/SocketProtocolError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "system/Error.hxx"
#include "util/SpanCast.hxx"
#include "util/Unaligned.hxx"

#ifdef HAVE_URING
#include "io/uring/Operation.hxx"
#include "io/uring/Queue.hxx"
#endif

#include <was/protocol.h>

#include <cassert>

namespace Was {

#ifdef HAVE_URING

class Control::UringSend final : Uring::Operation {
	Control &parent;
	Uring::Queue &queue;

	DefaultFifoBuffer buffer;

	bool canceled = false;

public:
	[[nodiscard]]
	UringSend(Control &_parent,
		  Uring::Queue &_queue) noexcept
		:parent(_parent), queue(_queue)
	{
	}

	void Start(DefaultFifoBuffer &src);

	void Cancel() noexcept;

private:
	void OnUringCompletion(int res) noexcept override;
};

void
Control::UringSend::Start(DefaultFifoBuffer &src)
{
	assert(!canceled);
	assert(parent.uring_send == this);

	if (IsUringPending())
		return;

	buffer.MoveFromAllowBothNull(src);
	src.FreeIfEmpty();

	const auto r = buffer.Read();
	if (r.empty()) {
		buffer.FreeIfEmpty();
		parent.OnUringSendDone(true);
		return;
	}

	auto &s = queue.RequireSubmitEntry();
	io_uring_prep_send(&s, parent.socket.GetSocket().Get(),
			   r.data(), r.size(), 0);

	/* always go async; this way, the overhead for the operation
           does not cause latency in the main thread */
	io_uring_sqe_set_flags(&s, IOSQE_ASYNC);

	queue.Push(s, *this);
}

void
Control::UringSend::Cancel() noexcept
{
	assert(!canceled);
	assert(parent.uring_send == this);

	parent.uring_send = nullptr;

	if (IsUringPending()) {
		canceled = true;

		auto &s = queue.RequireSubmitEntry();
		io_uring_prep_cancel(&s, GetUringData(), 0);
		io_uring_sqe_set_data(&s, nullptr);
		io_uring_sqe_set_flags(&s, IOSQE_CQE_SKIP_SUCCESS);
		queue.Submit();
	} else {
		delete this;
	}
}

void
Control::UringSend::OnUringCompletion(int res) noexcept
{
	if (canceled) [[unlikely]] {
		delete this;
		return;
	}

	assert(parent.uring_send == this);

	if (res < 0) [[unlikely]] {
		parent.OnUringSendError(-res);
		return;
	}

	buffer.Consume(res);
	parent.OnUringSendDone(buffer.empty());
}

void
Control::EnableUring(Uring::Queue &uring_queue)
{
	assert(uring_send == nullptr);
	assert(output_buffer.empty());

	socket.EnableUring(uring_queue);

	uring_send = new UringSend(*this, uring_queue);
}

inline void
Control::CancelUringSend() noexcept
{
	if (uring_send != nullptr) {
		uring_send->Cancel();
		assert(uring_send == nullptr);
	}
}

inline void
Control::OnUringSendDone(bool empty) noexcept
{
	if (!empty || !output_buffer.empty())
		uring_send->Start(output_buffer);
	else if (done)
		InvokeDone();
}

inline void
Control::OnUringSendError(int error) noexcept
{
	InvokeError(std::make_exception_ptr(MakeErrno(error, "Send failed")));
}

#endif

Control::Control(EventLoop &event_loop, UniqueSocketDescriptor &&_fd,
		 ControlHandler &_handler) noexcept
	:socket(event_loop), handler(&_handler)
{
	socket.Init(_fd.Release(), FD_SOCKET, write_timeout, *this);

	if (!socket.HasUring())
		socket.ScheduleRead();
}

Control::~Control() noexcept
{
#ifdef HAVE_URING
	CancelUringSend();
#endif
}

void
Control::ReleaseSocket() noexcept
{
	assert(socket.IsConnected());

#ifdef HAVE_URING
	CancelUringSend();
#endif

	output_buffer.FreeIfDefined();

	socket.Abandon();
	socket.Destroy();
}

void
Control::InvokeError(const char *msg) noexcept
{
	InvokeError(std::make_exception_ptr(SocketProtocolError{msg}));
}

BufferedResult
Control::OnBufferedData()
{
	if (done) {
		InvokeError("received too much control data");
		return BufferedResult::DESTROYED;
	}

	while (true) {
		auto r = socket.ReadBuffer();

		struct was_header header;
		if (r.size() < sizeof(header))
			break;

		LoadUnaligned(header, r.data());
		r = r.subspan(sizeof(header));
		if (r.size() < header.length)
			break;

		const auto payload = r.first(header.length);

		socket.KeepConsumed(sizeof(header) + payload.size());

		bool success = handler->OnWasControlPacket(was_command(header.command),
							   payload);
		if (!success)
			return BufferedResult::DESTROYED;
	}

	/* not enough data yet */
	if (!InvokeDrained())
		return BufferedResult::DESTROYED;

	return BufferedResult::MORE;
}

bool
Control::OnBufferedClosed() noexcept
{
	Close();
	handler->OnWasControlHangup();
	return false;
}

bool
Control::OnBufferedWrite()
{
#ifdef HAVE_URING
	if (uring_send != nullptr) {
		assert(!output_buffer.empty());
		uring_send->Start(output_buffer);
		return true;
	}
#endif

	auto r = output_buffer.Read();
	assert(!r.empty());

	ssize_t nbytes = socket.Write(r);
	if (nbytes <= 0) {
		switch (static_cast<write_result>(nbytes)) {
		case WRITE_SOURCE_EOF:
		case WRITE_BLOCKING:
			return true;

		case WRITE_ERRNO:
			throw MakeErrno("WAS control send error");

		case WRITE_DESTROYED:
		case WRITE_BROKEN:
			return false;
		}
	}

	output_buffer.Consume(nbytes);

	if (output_buffer.empty()) {
		output_buffer.Free();
		socket.UnscheduleWrite();

		if (done) {
			InvokeDone();
			return false;
		}
	} else
		socket.ScheduleWrite();

	return true;
}

bool
Control::OnBufferedDrained() noexcept
{
	return handler->OnWasControlDrained();
}

enum write_result
Control::OnBufferedBroken()
{
	throw SocketClosedPrematurelyError{"WAS control socket closed by peer"};
}

void
Control::OnBufferedError(std::exception_ptr e) noexcept
{
	InvokeError(e);
}

void
Control::Close() noexcept
{
#ifdef HAVE_URING
	CancelUringSend();
#endif

	if (socket.IsValid()) {
		socket.Close();
		socket.Destroy();
	}
}

/*
 * constructor
 *
 */

std::byte *
Control::Start(enum was_command cmd, size_t payload_length) noexcept
{
	assert(!done);

	const struct was_header header{
		.length = static_cast<uint16_t>(payload_length),
		.command = static_cast<uint16_t>(cmd),
	};

	output_buffer.AllocateIfNull();
	auto w = output_buffer.Write();
	if (w.size() < sizeof(header) + payload_length) {
		InvokeError("control output is too large");
		return nullptr;
	}

	StoreUnaligned(w.data(), header);
	return w.data() + sizeof(header);
}

void
Control::Finish(size_t payload_length) noexcept
{
	assert(!done);

	output_buffer.Append(sizeof(struct was_header) + payload_length);

	socket.DeferWrite();
}

bool
Control::Send(enum was_command cmd,
	      std::span<const std::byte> payload) noexcept
{
	assert(!done);

	std::byte *dest = Start(cmd, payload.size());
	if (dest == nullptr)
		return false;

	std::copy(payload.begin(), payload.end(), dest);
	Finish(payload.size());
	return true;
}

bool
Control::SendString(enum was_command cmd, std::string_view payload) noexcept
{
	return Send(cmd, AsBytes(payload));
}

bool
Control::SendPair(enum was_command cmd, std::string_view name,
		  std::string_view value) noexcept
{
	const std::size_t payload_size = name.size() + 1 + value.size();

	char *dest = reinterpret_cast<char *>(Start(cmd, payload_size));
	if (dest == nullptr)
		return false;

	dest = std::copy(name.begin(), name.end(), dest);
	*dest++ = '=';
	dest = std::copy(value.begin(), value.end(), dest);

	Finish(payload_size);
	return true;
}

bool
Control::SendArray(enum was_command cmd,
		   std::span<const char *const> values) noexcept
{
	for (auto value : values) {
		assert(value != nullptr);

		if (!SendString(cmd, value))
			return false;
	}

	return true;
}

void
Control::Done() noexcept
{
	assert(!done);

	done = true;

	if (!socket.IsEmpty()) {
		InvokeError("received too much control data");
		return;
	}

	if (output_buffer.empty())
		InvokeDone();
}

} // namespace Was
