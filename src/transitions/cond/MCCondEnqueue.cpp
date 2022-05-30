#include "MCCondEnqueue.h"
#include "MCMINI.h"
#include "transitions/mutex/MCMutexTransition.h"
#include "transitions/mutex/MCMutexUnlock.h"
#include "MCTransitionFactory.h"

MCTransition*
MCReadCondEnqueue(const MCSharedTransition *shmTransition, void *shmData, MCState *state)
{
    const auto shmCond = static_cast<MCSharedMemoryConditionVariable*>(shmData);
    const auto condInShm = shmCond->cond;
    const auto mutexInShm = shmCond->mutex;
    const auto condSystemId = (MCSystemID)condInShm;
    const auto mutexSystemId = (MCSystemID)mutexInShm;
    const auto condThatExists = state->getVisibleObjectWithSystemIdentity<MCConditionVariable>(condSystemId);
    const auto mutexThatExists = state->getVisibleObjectWithSystemIdentity<MCMutex>(mutexSystemId);

    MC_REPORT_UNDEFINED_BEHAVIOR_ON_FAIL(condThatExists != nullptr, "Attempting to wait on a condition variable that is uninitialized");
    MC_REPORT_UNDEFINED_BEHAVIOR_ON_FAIL(mutexThatExists != nullptr, "Attempting to wait on a condition variable with an uninitialized mutex");
    MC_REPORT_UNDEFINED_BEHAVIOR_ON_FAIL(!condThatExists->isDestroyed(), "Attempting to wait on a destroyed condition variable");

    const auto threadThatRanId = shmTransition->executor;
    const auto mutexAssociatedWithConditionVariable = condThatExists->mutex;
    
    if (mutexAssociatedWithConditionVariable != nullptr) {
        MC_REPORT_UNDEFINED_BEHAVIOR_ON_FAIL(*mutexThatExists == *mutexAssociatedWithConditionVariable,
                                               "A mutex has already been associated with this condition variable. Attempting "
                                               "to use another mutex with the same condition variable is undefined");
    }

    auto threadThatRan = state->getThreadWithId(threadThatRanId);
    return new MCCondEnqueue(threadThatRan, condThatExists, mutexThatExists);
}

std::shared_ptr<MCTransition>
MCCondEnqueue::staticCopy()
{
    auto threadCpy=
            std::static_pointer_cast<MCThread, MCVisibleObject>(this->thread->copy());
    auto condCpy =
            std::static_pointer_cast<MCConditionVariable, MCVisibleObject>(this->conditionVariable->copy());
    auto mutCpy =
            std::static_pointer_cast<MCMutex, MCVisibleObject>(this->mutex->copy());
    auto cpy = new MCCondEnqueue(threadCpy, condCpy, mutCpy);
    return std::shared_ptr<MCTransition>(cpy);
}

std::shared_ptr<MCTransition>
MCCondEnqueue::dynamicCopyInState(const MCState *state)
{
    auto threadInState = state->getThreadWithId(thread->tid);
    auto condInState = state->getObjectWithId<MCConditionVariable>(conditionVariable->getObjectId());
    auto mutCpy = state->getObjectWithId<MCMutex>(this->mutex->getObjectId());
    auto cpy = new MCCondEnqueue(threadInState, condInState, mutCpy);
    return std::shared_ptr<MCTransition>(cpy);
}

void
MCCondEnqueue::applyToState(MCState *state)
{
    /* Insert this thread into the waiting queue */
    this->conditionVariable->enterSleepingQueue(this->getThreadId());
    this->conditionVariable->mutex = this->mutex;
    this->mutex->unlock();
}

bool
MCCondEnqueue::coenabledWith(std::shared_ptr<MCTransition> other)
{
    auto maybeCondWaitOperation = std::dynamic_pointer_cast<MCCondEnqueue, MCTransition>(other);
    if (maybeCondWaitOperation) {
        /* Only one cond_wait will be able to acquire the mutex */
        return *maybeCondWaitOperation->conditionVariable != *this->conditionVariable;
    }

    auto maybeMutexOperation = std::dynamic_pointer_cast<MCMutexTransition, MCTransition>(other);
    if (maybeMutexOperation) {
        auto unlockMutex = std::make_shared<MCMutexUnlock>(this->thread, this->conditionVariable->mutex);
        return MCTransitionFactory::transitionsCoenabledCommon(unlockMutex, maybeMutexOperation);
    }

    return true;
}

bool
MCCondEnqueue::dependentWith(std::shared_ptr<MCTransition> other)
{
    auto maybeCondOperation = std::dynamic_pointer_cast<MCCondTransition, MCTransition>(other);
    if (maybeCondOperation) {
        return *maybeCondOperation->conditionVariable == *this->conditionVariable;
    }

    auto maybeMutexOperation = std::dynamic_pointer_cast<MCMutexTransition, MCTransition>(other);
    if (maybeMutexOperation) {
        auto unlockMutex = std::make_shared<MCMutexUnlock>(this->thread, this->conditionVariable->mutex);
        return MCTransitionFactory::transitionsCoenabledCommon(unlockMutex, maybeMutexOperation);
    }
    return false;
}

void
MCCondEnqueue::print()
{
    printf("thread %lu: pthread_cond_wait(%lu, %lu) (awake)\n", this->thread->tid, this->conditionVariable->getObjectId(), this->mutex->getObjectId());
}