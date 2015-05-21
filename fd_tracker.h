#ifndef FD_TRACKER_H
#define FD_TRACKER_H

#include <log/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include <utils/CallStack.h>
#include <utils/String8.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>

struct entry_points {
#define ENTRYPOINT_ENUM(name, rettype, ...) rettype ( * p_##name )( __VA_ARGS__ );

#include "entry_points.h"
    ENTRYPOINT_LIST(ENTRYPOINT_ENUM);

#undef ENTRYPOINT_LIST
#undef ENTRYPOINT_ENUM
} g_entry_points;

enum tracking_mode {
    DISABLED,
    NOT_TRIGGERED,
    TRIGGERED,
};

typedef struct trace_info_ {
    int count;
    char* native_stack_trace;
    char* java_stack_trace;  
} trace_info;

int str_hash(void *key);
bool str_equals(void *key_a, void *key_b);

char* md5 (char * data, char * data2);

class AutoLock {
public:
    AutoLock(pthread_mutex_t * mutex) {
        this->m_mutex = mutex;
        pthread_mutex_lock(m_mutex);
    }

    ~AutoLock() {
        pthread_mutex_unlock(m_mutex);
    }

private:
    pthread_mutex_t * m_mutex;
};

#endif  // FD_TRACKER_H
