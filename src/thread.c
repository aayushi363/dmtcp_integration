#include "thread.h"

MEMORY_ALLOC_DEF_DECL(thread);
PRETTY_PRINT_DEF_DECL(thread);
MEMORY_ALLOC_DEF_DECL(thread_operation);
PRETTY_PRINT_DEF_DECL(thread_operation);

thread_ref
thread_self(void)
{
    return thread_create(pthread_self());
}

thread_ref
thread_create(pthread_t pthread) {
    thread_ref thread = thread_alloc();
    if (!thread) return NULL;
    thread->is_alive = true;
    thread->start_routine = NULL;
    thread->arg = NULL;
    thread->owner = pthread;
    return thread; // owner unspecified
}

thread_ref
thread_copy(thread_refc other)
{
    thread_ref thread = thread_alloc();
    if (!thread) return NULL;
    thread->is_alive = other->is_alive;
    thread->start_routine = other->start_routine;
    thread->arg = other->arg;
    thread->owner = other->owner;
    return thread;
}

void
thread_destroy(thread_ref thread)
{
    if (!thread) return;
    free(thread);
}

thread_operation_ref
thread_operation_copy(thread_operation_refc ref)
{

}

void
thread_operation_destroy(thread_operation_ref ref)
{
    if (!ref) return;
    thread_destroy(ref->thread);
    free(ref);
}

void
thread_operation_pretty_off(thread_operation_refc t, unsigned int off)
{

}

bool
threads_equal(thread_refc t1, thread_refc t2)
{
    if (t1 == NULL) return t2 == NULL;
    if (t2 == NULL) return t1 == NULL;
    return pthread_equal(t1->owner, t2->owner);
}

bool
thread_operation_enabled(thread_operation_refc top, thread_refc thread)
{
    if (!top || !thread) return false;
    switch (top->type) {
        case THREAD_JOIN:
            return !top->thread->is_alive;
        case THREAD_FINISH:
            return false;
        default:
            return true;
    }
}