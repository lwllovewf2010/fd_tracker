#include "fd_tracker.h"
#include <assert.h>
#include <strings.h>

volatile tracking_mode g_tracking_mode = DISABLED;
struct tracking_info** g_tracking_info = NULL;

__attribute__((constructor))
void setup() {
    ALOGE("FD_TRACKER: setup");
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
    g_tracking_info = (struct tracking_info**) malloc(sizeof(struct tracking_info*) * (limit.rlim_cur + 1));
    bzero(g_tracking_info, sizeof(sizeof(struct tracking_info*) * limit.rlim_cur));
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
    assert(fd >= 0);
    android::CallStack stack;
    stack.update(3);

    if (g_tracking_info[fd] != 0) {
        if (g_tracking_info[fd]->trace != NULL) {
            free(g_tracking_info[fd]->trace);
        }
        free(g_tracking_info[fd]);
    } 

    struct tracking_info * info = (struct tracking_info *) malloc(sizeof(struct tracking_info));
    info->fd = fd;
    info->trace = strdup(stack.toString(""));
    info->time = time(0);
    
    g_tracking_info[fd] = info;        
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
    struct tracking_info ** info = g_tracking_info;
    ALOGE("FD_TRACKER: ------ dump begin ------");
    while (*info != NULL) {
        ALOGE("FD_TRACKER: fd: %d", (*info)->fd);
        ALOGE("FD_TRACKER: trace: %s", (*info)->trace);
        info++;
    }
    ALOGE("FD_TRACKER: ------ dump end------");
}

#define TRACK(name,...)                                         \
    do {                                                        \
        ALOGE("FD_TRACKER: TRACK %s,", #name);                 \
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
        int ret = (*g_entry_points.p_close)(fd);
        return ret;
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
