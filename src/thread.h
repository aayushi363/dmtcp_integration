#ifndef DPOR_THREAD_H
#define DPOR_THREAD_H

#include <pthread.h>
#include "array.h"
#include "hashtable.h"
#include "common.h"
#include "decl.h"

typedef uint64_t tid_t;

STRUCT_DECL(thread);
STATES_DECL(thread, THREAD_ALIVE, THREAD_SLEEPING, THREAD_DEAD);

PRETTY_PRINT_STATE_DECL(thread);
struct thread {
    tid_t tid;
    pthread_t owner;
    void * volatile arg;
    thread_routine start_routine;
    thread_state state;
};
PRETTY_PRINT_DECL(thread);
typedef array_ref thread_array_ref;
hash_t thread_hash(thread_ref);

STRUCT_DECL(thread_operation)
TYPES_DECL(thread_operation,
           THREAD_START,
           THREAD_CREATE,
           THREAD_JOIN,
           THREAD_FINISH,
           THREAD_TERMINATE_PROCESS
           );
struct thread_operation {
    thread_operation_type type;
    csystem_local thread thread;
};

// Dynamism for thread operations is already existant
STRUCT_DECL(dynamic_thread_operation)
struct dynamic_thread_operation {
    thread_operation_type type;
    csystem_local thread_ref thread;
};

PRETTY_PRINT_DECL(thread_operation);
PRETTY_PRINT_TYPE_DECL(thread_operation);
/*
 * Operations
 */
thread_ref thread_get_self(void);
thread_ref thread_get_main_thread(void);
bool threads_equal(thread_refc, thread_refc);
bool thread_operation_spawns_thread(thread_refc, thread_operation_refc);
bool thread_operation_joins_thread(thread_refc, thread_operation_refc);
bool thread_operation_enabled(thread_operation_refc, thread_refc);

#endif
