#include "fd_tracker.h"
#include <assert.h>
#include <strings.h>
#include <runtime.h>
#include <cutils/hashmap.h>
#include <stdlib.h>

#define TRACK_THRESHHOLD 0.8
#define RESERVED_CALL_STACK_FD 1
volatile tracking_mode g_tracking_mode = DISABLED;

int g_rlimit_nofile = -1;
char** g_hash_array = NULL;
Hashmap * g_hash_map = NULL;
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
struct entry_points g_entry_points;

pthread_key_t g_key;

// FIXME: consider std::atomic for performance ?
// FIXME: doesn't work for setuid/setgid
// FIXME: 64-bit
volatile int g_setup_invoked = 0;
void setup() {
    {
        AutoLock lock(&g_mutex);
        if (g_setup_invoked == 1) {
            return;
        }
#define ENTRYPOINT_ENUM(name, rettype, ...)                             \
        typedef rettype (*FUNC_##name)(__VA_ARGS__);                    \
        g_entry_points.p_##name = (FUNC_##name) dlsym(RTLD_NEXT, #name); \

#include "entry_points.h"
        ENTRYPOINT_LIST(ENTRYPOINT_ENUM);

#undef ENTRYPOINT_LIST
#undef ENTRYPOINT_ENUM
        pthread_key_create(&g_key,NULL);
        g_setup_invoked = 1;
    }
    struct rlimit limit;
    GET_RLIMIT(limit);

    g_rlimit_nofile = limit.rlim_cur - RESERVED_CALL_STACK_FD;

    assert(TRACK_THRESHHOLD > 0 && TRACK_THRESHHOLD < 1);

    limit.rlim_cur = (rlim_t) (g_rlimit_nofile * TRACK_THRESHHOLD);
    int ret = setrlimit(RLIMIT_NOFILE, &limit);
    if (ret == -1) {
        ALOGE("FD_TRACKER: setrlimit failed, errno: %d", errno);
        return;
    }
    g_tracking_mode = NOT_TRIGGERED;

    g_hash_array = (char**) malloc(sizeof(char*) * (g_rlimit_nofile));
    bzero(g_hash_array, sizeof(char*) * g_rlimit_nofile);
    g_hash_map = hashmapCreate(g_rlimit_nofile, pred_str_hash, pred_str_equals);
}

void do_track(int fd) {
    AutoLock lock(&g_mutex);
    if (g_tracking_mode != TRIGGERED) {
        return;
    };
    assert(fd >= 0);
    if (fd >= g_rlimit_nofile) {
        ALOGE("FD_TRACKER: fd: %d exceed rlimit: %d?", fd, g_rlimit_nofile);
        return;
    }

    struct rlimit limit;
    GET_RLIMIT(limit);

    if (limit.rlim_cur != (rlim_t) g_rlimit_nofile) {
        ALOGE("FD_TRACKER: RLIMIT seems changed outside, DISABLE");
        g_tracking_mode = DISABLED;
        return;
    }
    
    limit.rlim_cur = g_rlimit_nofile + RESERVED_CALL_STACK_FD;
    int ret = setrlimit(RLIMIT_NOFILE, &limit);
    if (ret == -1) {
        ALOGE("FD_TRACKER: setrlimit failed, errno: %d", errno);
        return;
    }

    android::CallStack stack;
    stack.update(4);

    std::ostringstream java_stack;
    art::Runtime::DumpJavaStack(java_stack);

    limit.rlim_cur = g_rlimit_nofile;
    ret = setrlimit(RLIMIT_NOFILE, &limit);
    if (ret == -1) {
        ALOGE("FD_TRACKER: setrlimit failed, errno: %d", errno);
        return;
    }

    assert(g_hash_array[fd] == NULL);

    char* md5_sum = md5((char*)stack.toString("").string(), (char*)java_stack.str().c_str());

    trace_info * _trace_info = (trace_info *) hashmapGet(g_hash_map,md5_sum);
    if (_trace_info == NULL) {
        _trace_info = (trace_info *) malloc(sizeof(trace_info));
        _trace_info->count = 1;
        _trace_info->native_stack_trace = strdup(stack.toString("").string());
        _trace_info->java_stack_trace = strdup(java_stack.str().c_str());
        hashmapPut(g_hash_map, md5_sum, _trace_info);
    } else {
        _trace_info->count++;
    }
    g_hash_array[fd] = md5_sum;
}

void do_trigger() {
    AutoLock lock(&g_mutex);
    if (g_tracking_mode != NOT_TRIGGERED) {
        return;
    };

    struct rlimit limit;
    GET_RLIMIT(limit);

    if (limit.rlim_cur != (rlim_t) (g_rlimit_nofile * TRACK_THRESHHOLD)) {
        ALOGE("FD_TRACKER: RLIMIT seems changed outside, DISABLE");
        g_tracking_mode = DISABLED;
        return;
    }
    limit.rlim_cur = (rlim_t) (g_rlimit_nofile);
    int ret = setrlimit(RLIMIT_NOFILE, &limit);
    if (ret == -1) {
        ALOGE("FD_TRACKER: setrlimit failed, errno: %d", errno);
        g_tracking_mode = DISABLED;
        return;
    }
    ALOGE("FD_TRACKER: reset RLIM_NOFILE to %d", limit.rlim_cur);
    g_tracking_mode = TRIGGERED;
}

void do_report() {
    AutoLock lock(&g_mutex);

    int hash_size = hashmapSize(g_hash_map);
    trace_info * traces [hash_size];
    bzero(traces, sizeof (trace_info *) * hash_size);

    int context [2] = {(int)traces, 0};
    hashmapForEach(g_hash_map, pred_collect_map_value, (void *) context);

    qsort(traces, hash_size, sizeof(trace_info *), pred_sort_trace);

    ALOGE("FD_TRACKER: ****** dump begin ******");
    for (int i = 0; i < hash_size; i++) {
        trace_info * _trace_info = traces[i];
        ALOGE("FD_TRACKER: ------ dump trace ------");
        ALOGE("FD_TRACKER: repetition: %d", _trace_info->count);
        ALOGE("FD_TRACKER: java trace:\n%s", _trace_info->java_stack_trace);
        ALOGE("FD_TRACKER: native trace:\n%s", _trace_info->native_stack_trace);
    }
    ALOGE("FD_TRACKER: ****** dump end ******");
    
    g_tracking_mode = DISABLED;
}

void do_close(int fd) {
    AutoLock lock(&g_mutex);
    if (g_tracking_mode != TRIGGERED) {
        return;
    }

    char * md5_sum = g_hash_array[fd];
    if (md5_sum == NULL) {
        return;
    }
            
    trace_info * _trace_info = (trace_info *) hashmapGet(g_hash_map, md5_sum);
    assert(_trace_info != NULL);
            
    _trace_info->count--;
    if (_trace_info->count == 0) {
        free(_trace_info->native_stack_trace);
        free(_trace_info->java_stack_trace);
        hashmapRemove(g_hash_map, md5_sum);
        free(md5_sum);
    }
    g_hash_array[fd] = NULL;
}

extern "C" {
    int close(int fd) {
        if (g_setup_invoked == 0) {
            setup();
        }
        int ret = (*g_entry_points.p_close)(fd);
        if (g_tracking_mode != TRIGGERED) {
            return ret;
        }
        if (ret == -1) {
            return ret;
        }
        if (fd < 0 || fd >= g_rlimit_nofile) {
            return ret;
        }

        int * is_recursive = (int *) pthread_getspecific(g_key);
        if (is_recursive != NULL && *is_recursive == 1) {
            return ret;
        }
        
        do_close(fd);
        return ret;
    }

    int open(const char *pathname, int flags, ...) {
        mode_t mode = 0;
        if ((flags & O_CREAT) != 0) {
            va_list args;
            va_start(args, flags);
            mode = (mode_t) va_arg(args, int);
            va_end(args);
            TRACK_RET(open, pathname, flags, mode);
        } else {
            TRACK_RET(open, pathname, flags);
        }
    }

    // opendir/closedir is not tracked, since they are implemented
    // using open/close

    int socket(int domain, int type, int protocol) {
        TRACK_RET (socket, domain, type, protocol);
    }

    int socketpair(int domain, int type, int protocol, int array[2]) {
        TRACK_ARRAY(socketpair, domain, type, protocol, array);
    }

    int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
        TRACK_RET (accept, sockfd, addr, addrlen);
    }

    int dup(int oldfd) {
        TRACK_RET(dup, oldfd);
    }

    int dup2 (int oldfd, int newfd) {
        TRACK_RET(dup2, oldfd, newfd);
    }

    int dup3 (int oldfd, int newfd, int flag) {
        TRACK_RET(dup3, oldfd, newfd, flag);
    }

    int pipe (int array[2]) {
        TRACK_ARRAY(pipe, array);
    }

    int pipe2 (int array[2], int flags) {
        TRACK_ARRAY(pipe2, array, flags);
    }

    int creat(const char *path, mode_t mod) {
        TRACK_RET(creat, path, mod);
    }
}
