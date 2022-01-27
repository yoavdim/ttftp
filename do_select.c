#include <sys/time.h> // extracted to a seperate file for #include debugging

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include "do_select.h"
int do_select(int sock, int secs) {
    struct timeval tv;
    fd_set wr, rd, ex;
    int res;
    tv.tv_sec = secs;
    tv.tv_usec = 0;
    FD_ZERO(&wr);
    FD_ZERO(&ex);
    FD_ZERO(&rd);
    FD_SET(sock, &rd);
    res = select(sock+1, &rd, &wr, &ex, &tv);
    if((res < 0) && (errno != EINTR))
        return -1;
    return res > 0;
}
