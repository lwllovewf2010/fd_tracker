#define ENTRYPOINT_LIST(V)                      \
    V(close, int, int)                              \
    V(open, int, const char *, int)                  \
    V (socket, int, int, int, int)                     \
    V (accept, int, int, struct sockaddr*, socklen_t*) \
    V (dup, int, int)                                  \
    V (dup2, int, int, int)                            \
    V (dup3, int, int, int, int)                           \

