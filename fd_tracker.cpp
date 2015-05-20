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

tracking_mode g_tracking_mode = DISABLED;

void setup() {
#define ENTRYPOINT_ENUM(name, rettype, ...)                             \
    typedef rettype (*FUNC_##name)(__VA_ARGS__);                        \
    g_entry_points.p_##name = (FUNC_##name) dlsym(RTLD_NEXT, #name);    \

#include "entry_points.h"
    ENTRYPOINT_LIST(ENTRYPOINT_ENUM);

#undef ENTRYPOINT_LIST
#undef ENTRYPOINT_ENUM

    struct rlimit limit;
    int ret = getrlimit(RLIMIT_NOFILE, &limit);
    if (ret) {
        ALOGE("FD_TRACKER: getrlimit failed, errno: %d", errno);
        return;
    }
    if (limit.rlim_cur == RLIM_INFINITY) {
        ALOGE("FD_TRACKER: RLIM_NOFILE is INFINITY, skip fd_tracker");
        return;
    }
    limit.rlim_cur = (rlim_t) ((limit.rlim_cur / 3.0) * 2);
    ret = setrlimit(RLIMIT_NOFILE, &limit);
    if (ret) {
        ALOGE("FD_TRACKER: setrlimit failed, errno: %d", errno);
        return;
    }
    ALOGE("FD_TRACKER: reset RLIM_NOFILE to %d", limit.rlim_cur);
    g_tracking_mode = NOT_TRIGGERED;
}

void do_track(int fd) {
    int x = fd;
    android::CallStack stack;
    stack.update(3);
    android::String8 s = stack.toString("");
}

void do_trigger() {
    struct rlimit limit;
    int ret = getrlimit(RLIMIT_NOFILE, &limit);
    if (ret) {
        ALOGE("FD_TRACKER: getrlimit failed, errno: %d", errno);
        g_tracking_mode = DISABLED;
        return;
    }
    if (limit.rlim_cur == RLIM_INFINITY) {
        ALOGE("FD_TRACKER: RLIM_NOFILE is INFINITY, skip fd_tracker");
        g_tracking_mode = DISABLED;
        return;
    }
    limit.rlim_cur = (rlim_t) ((limit.rlim_cur / 2) * 3);
    ret = setrlimit(RLIMIT_NOFILE, &limit);
    if (ret) {
        ALOGE("FD_TRACKER: setrlimit failed, errno: %d", errno);
        g_tracking_mode = DISABLED;
        return;
    }
    ALOGE("FD_TRACKER: reset RLIM_NOFILE to %d", limit.rlim_cur);
    g_tracking_mode = TRIGGERED;
}

void do_report() {

}

#define TRACK(name,...)                                         \
    do {                                                        \
        int ret = (*g_entry_points.p_##name)(__VA_ARGS__);      \
        if (g_tracking_mode == TRIGGERED) {                     \
            do_track(ret);                                      \
        }                                                       \
        if (ret == -1 && errno == EMFILE) {                      \
            if (g_tracking_mode == NOT_TRIGGERED) {             \
                do_trigger();                                   \
                ret = (*g_entry_points.p_##name)(__VA_ARGS__);  \
            } else if (g_tracking_mode == TRIGGERED) {          \
                do_report();                                    \
            }                                                   \
        }                                                       \
        return ret;                                             \
    } while (0)                                                 \

extern "C" {
    int close(int fd) {
        TRACK(close,fd);
    }

    int open(const char *pathname, int flags) {
        TRACK(open, pathname, flags);
    }

    // opendir/closedir is not tracked, since they are implemented
    // using open/close

    int socket(int domain, int type, int protocol) {
        TRACK (socket, domain, type, protocol);
    }
    
    int socketpair(int domain, int type, int protocol, int sv[2]) {
        TRACK(socketpair, domain, type, protocol, sv);
    }
    
    int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
        TRACK (accept, sockfd, addr, addrlen);
    }

    int dup(int oldfd) {
        TRACK(dup, oldfd);
    }

    int dup2 (int oldfd, int newfd) {
        TRACK(dup2, oldfd, newfd);
    }

    int dup3 (int oldfd, int newfd, int flag) {
        TRACK(dup3, oldfd, newfd, flag);
    }
    
    int pipe (int fd[2]) {
        TRACK(pipe, fd);
    }
    
    int pipe2 (int fd[2], int flags) {
        TRACK(pipe2, fd, flags);
    }

    int creat(const char *path, mode_t mod) {
        TRACK(creat, path, mod);
    }

}
