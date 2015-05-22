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
};

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

int pred_str_hash(void *key);
bool pred_str_equals(void *key_a, void *key_b);
int pred_sort_trace(const void * t1, const void * t2);
bool pred_collect_map_value (void * key, void * value, void * context);

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

#define DO_TRACK_RET                                \
    do {                                        \
        do_track(ret);                          \
    } while (0)                                 \

#define DO_TRACK_ARRAY                          \
    do {                                        \
        do_track(array[0]);                     \
        do_track(array[1]);                     \
    } while (0)                                 \

#define TRACK_RET(name,...)                     \
    TRACK(DO_TRACK_RET, name, __VA_ARGS__)          \

#define TRACK_ARRAY(name,...)                   \
    TRACK(DO_TRACK_ARRAY, name, __VA_ARGS__)    \

#define TRACK(DO_TRACK,name,...)                                \
    do {                                                        \
        int ret = (*g_entry_points.p_##name)(__VA_ARGS__);      \
        int orig_errno = errno;                                 \
        if (ret == -1 && orig_errno == EMFILE) {                 \
            if (g_tracking_mode == NOT_TRIGGERED) {             \
                do_trigger();                                   \
                ret = (*g_entry_points.p_##name)(__VA_ARGS__);  \
            } else if (g_tracking_mode == TRIGGERED) {          \
                do_report();                                    \
            }                                                   \
            errno = orig_errno;                                 \
        } else {                                                \
            if (g_tracking_mode == TRIGGERED) {                 \
                DO_TRACK;                                       \
            }                                                   \
        }                                                       \
        return ret;                                             \
    } while (0)                                                 \
        
#endif  // FD_TRACKER_H
