#ifndef MC_MCTHREAD_H
#define MC_MCTHREAD_H

#include <pthread.h>
#include "MCVisibleObject.h"
#include "MCShared.h"
#include <memory>

struct MCThreadShadow {
    void * arg;
    thread_routine startRoutine;
    pthread_t systemIdentity;
    enum MCThreadState {
        embryo, alive, sleeping, dead
    } state;

    MCThreadShadow(void *arg, thread_routine startRoutine, pthread_t systemIdentity) :
            arg(arg), startRoutine(startRoutine), systemIdentity(systemIdentity), state(embryo) {}
};

struct MCThread : public MCVisibleObject {
private:
    MCThreadShadow threadShadow;

    bool _hasEncounteredProgressGoal = false;
    bool _maybeStarved = false;

public:
    /* Threads are unique in that they have *two* ids */
    const tid_t tid;

    inline
    MCThread(tid_t tid, void *arg, thread_routine startRoutine, pthread_t systemIdentity) :
    MCVisibleObject(), threadShadow(MCThreadShadow(arg, startRoutine, systemIdentity)), tid(tid) {}

    inline explicit MCThread(tid_t tid, MCThreadShadow shadow) : MCVisibleObject(), threadShadow(shadow), tid(tid) {}
    inline MCThread(const MCThread &thread)
    : MCVisibleObject(thread.getObjectId()), threadShadow(thread.threadShadow), tid(thread.tid),
      _maybeStarved(thread._maybeStarved) {}

    std::shared_ptr<MCVisibleObject> copy() override;
    MCSystemID getSystemId() override;

    bool operator ==(const MCThread&) const;

    // Managing thread state
    MCThreadShadow::MCThreadState getState() const;

    bool enabled() const;
    bool isAlive() const;
    bool isDead() const;

    void awaken();
    void sleep();

    void regenerate();
    void die();
    void spawn();
    void despawn();

    inline void
    markThreadAsLive()
    {
        _maybeStarved = false;
    }

    inline void
    markThreadAsMaybeStarved()
    {
        _maybeStarved = true;
    }

    inline bool
    isThreadStarved() const
    {
        return _maybeStarved && !_hasEncounteredProgressGoal;
    }

};

#endif //MC_MCTHREAD_H
