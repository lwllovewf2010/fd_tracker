#define ENTRYPOINT_LIST(V)                              \
    V(close, int, int)                                  \
    V(open, int, const char *, int)                     \
    V(creat, int, const char *, mode_t)                 \
    V (socket, int, int, int, int)                      \
    V (socketpair, int, int, int, int, int[2])          \
    V (accept, int, int, struct sockaddr*, socklen_t*)  \
    V (dup, int, int)                                   \
    V (dup2, int, int, int)                             \
    V (dup3, int, int, int, int)                        \
    V (pipe, int, int[2])                               \
    V (pipe2, int, int[2], int)                         \


