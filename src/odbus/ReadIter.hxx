/*
 * Copyright 2007-2017 Content Management AG
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

#ifndef ODBUS_READ_ITER_HXX
#define ODBUS_READ_ITER_HXX

#include "Iter.hxx"

namespace ODBus {

class ReadMessageIter : public MessageIter {
	struct RecurseTag {};
	ReadMessageIter(RecurseTag, ReadMessageIter &parent) noexcept {
		dbus_message_iter_recurse(&parent.iter, &iter);
	}

public:
	explicit ReadMessageIter(DBusMessage &msg) noexcept {
		dbus_message_iter_init(&msg, &iter);
	}

	bool HasNext() noexcept {
		return dbus_message_iter_has_next(&iter);
	}

	bool Next() noexcept {
		return dbus_message_iter_next(&iter);
	}

	int GetArgType() noexcept {
		return dbus_message_iter_get_arg_type(&iter);
	}

	const char *GetSignature() noexcept {
		return dbus_message_iter_get_signature(&iter);
	}

	void GetBasic(void *value) noexcept {
		dbus_message_iter_get_basic(&iter, value);
	}

	const char *GetString() noexcept {
		const char *value;
		GetBasic(&value);
		return value;
	}

	/**
	 * Create a new iterator which recurses into a container
	 * value.
	 */
	ReadMessageIter Recurse() noexcept {
		return {RecurseTag(), *this};
	}
};

} /* namespace ODBus */

#endif
