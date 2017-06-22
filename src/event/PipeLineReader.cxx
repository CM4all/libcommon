/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "PipeLineReader.hxx"

#include <string.h>

void
PipeLineReader::TryRead(bool flush)
{
    assert(!buffer.IsFull());

    auto w = buffer.Write();
    assert(!w.IsEmpty());

    auto nbytes = fd.Read(w.data, w.size);
    if (nbytes <= 0) {
        event.Delete();
        callback(nullptr);
        return;
    }

    buffer.Append(nbytes);

    while (true) {
        auto r = buffer.Read();
        char *newline = (char *)memchr(r.data, '\n', r.size);
        if (newline == nullptr)
            break;

        buffer.Consume(newline + 1 - r.data);

        while (newline > r.data && newline[-1] == '\r')
            --newline;

        r.size = newline - r.data;
        if (!callback(r))
            return;
    }

    if (flush || buffer.IsFull()) {
        auto r = buffer.Read();
        buffer.Clear();

        if (!r.IsEmpty())
            callback(r);
    }
}
