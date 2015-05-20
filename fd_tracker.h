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

struct tracking_info {
    int fd;
    time_t time;
    char* trace;
};
#endif  // FD_TRACKER_H
