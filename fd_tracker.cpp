#include "fd_tracker.h"
#include <assert.h>
#include <strings.h>

volatile tracking_mode g_tracking_mode = DISABLED;
struct tracking_info** g_tracking_info = NULL;
int g_rlimit_nofile = -1;

__attribute__((constructor))
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

    g_rlimit_nofile = limit.rlim_cur;
    g_tracking_info = (struct tracking_info**) malloc(sizeof(struct tracking_info*) * (limit.rlim_cur));
    bzero(g_tracking_info, sizeof(struct tracking_info*) * limit.rlim_cur);
    
    limit.rlim_cur = (rlim_t) ((limit.rlim_cur / 5) * 4);
    ret = setrlimit(RLIMIT_NOFILE, &limit);
    if (ret) {
        ALOGE("FD_TRACKER: setrlimit failed, errno: %d", errno);
        return;
    }
    g_tracking_mode = NOT_TRIGGERED;
}

void do_track(int fd) {
    assert(g_tracking_mode = TRIGGERED);
    assert(fd >= 0);
    if (fd >= g_rlimit_nofile) {
        ALOGE("FD_TRACKER: fd: %d exceed rlimit: %d?", fd, g_rlimit_nofile);
        return;
    }
    
    struct rlimit limit;
    int ret = getrlimit(RLIMIT_NOFILE, &limit);
    int orig_limit = limit.rlim_cur;
    limit.rlim_cur = orig_limit+50;
    ret = setrlimit(RLIMIT_NOFILE, &limit);
    
    android::CallStack stack;
    stack.update(3);

    limit.rlim_cur = orig_limit;
    ret = setrlimit(RLIMIT_NOFILE, &limit);
    

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
    // ALOGE("FD_TRACKER: %d tracked", fd);
}

void do_trigger() {
    assert (g_tracking_mode == NOT_TRIGGERED);
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
    limit.rlim_cur = (rlim_t) (g_rlimit_nofile);
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
    ALOGE("FD_TRACKER: ------ dump begin ------");
    // assert(g_tracking_mode == TRIGGERED);
    struct tracking_info * info = NULL;
    for (int i = 0; i<g_rlimit_nofile; ++i) {
        info = g_tracking_info[i];
        if (info == NULL) {
            continue;
        }
        ALOGE("FD_TRACKER: fd: %d", info->fd);
        ALOGE("FD_TRACKER: trace: %s", info->trace);
    }
    ALOGE("FD_TRACKER: ------ dump end------");
}

#define TRACK(name,...)                                         \
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
                do_track(ret);                                  \
            }                                                   \
        }                                                       \
        return ret;                                             \
    } while (0)                                                 \

extern "C" {
    int close(int fd) {
        int ret = (*g_entry_points.p_close)(fd);
        if (g_tracking_mode != TRIGGERED) {
            return ret;
        }
        if (fd < 0 || fd >= g_rlimit_nofile) {
            return ret;
        }
        if (g_tracking_info[fd] != NULL) {
            if (g_tracking_info[fd]->trace != NULL) {
                free(g_tracking_info[fd]->trace);
            }
            free(g_tracking_info[fd]);
            g_tracking_info[fd] = NULL;
        } 
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
