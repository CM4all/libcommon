/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef SPAWN_HOOK_HXX
#define SPAWN_HOOK_HXX

struct PreparedChildProcess;

class SpawnHook {
public:
    virtual bool Verify(const PreparedChildProcess &) {
        return false;
    }
};

#endif
