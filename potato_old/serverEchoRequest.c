#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "server.h"

// Connection callbacks return values:
// returns -1 on error and close
// returns  1 on no error and close
// returns  0 on no error and keep



#define MAX_READ  1024

// Nothing that good, this is just for testing.
struct Buf
{
    char buf[MAX_READ + 1];
    size_t w/*written to so far*/, len/*length to write*/;
};

static // callback
int echoCleanup(struct POServer_connection *conn, struct Buf *buffer)
{
    DASSERT(conn);
    DASSERT(conn->fd >= 0);
    DASSERT(buffer);
    free(buffer);
    DLOG("\n");
    return 0; // success
}

// We doubt that the write will return with errno == EWOULDBLOCK, but it
// is possible that it can.  If we where writing a large amount of data
// then it would very likely return with errno == EWOULDBLOCK for some
// write() calls.  That is the nature of nonblocking read and write.
// This is very close to the robust way to do this.

static // callback
int echoWrite(struct POServer_connection *conn, struct Buf *buffer)
{
    DASSERT(buffer);
    DASSERT(buffer->len);

    int fd;
    size_t n, w;
    char *buf;
    buf = buffer->buf;
    w = buffer->w;
    fd = conn->fd;
    n = buffer->len;
    
    while(n)
    {
        ssize_t rt;

        rt = write(fd, &buf[w], n);

        if(rt > 0)
        {
            if(rt < n)
            {
                // We exhausted the epoll event for this stream
                buffer->w = (w += rt);
                buffer->len = (n -= rt);
                return 0; // keep this callback
            }

            DASSERT(rt == n);
            // add back the read callback
            // remove this write callback after we restored the read
            // callback, the epoll event stays triggered is it is.
            poServer_setWriteCallback(conn, NULL, NULL);

            DASSERT(conn->readCallback == NULL);
            // This will call the read callback when the socket is ready
            // for reading.
            return poServer_setReadCallback(conn,
                    poServer_echoRequest, buffer);
        }
        
        if(rt == -1)
        {
            DLOG("write(fd=%d,,) failed\n", fd);
            switch(errno)
            {
                case EWOULDBLOCK:
                    // This is how we know we are done with this non-blocking
                    // write.  Then again, rt < n should have caught this
                    // above.
                    return 0;
                    break;
                case EINTR: //interrupted by a signal
                    continue; // try write() again
                    break;
                case EPIPE: // "Broken pipe"
                case ECONNRESET: // "Connection reset by peer"
                    return 1;
                    break;
                default:
                    VASSERT(0, "read(fd=%d,,) failed unexpected errno\n", fd);
                    // Unexpected error, write more code here.
                    return -1;
                    break;
            }
        }
        else if(rt == 0)
        {
            // fd is closed, there's nothing wrong it's just closed.
            return 1;
        }
     }

    // keep GCC from complaining.
    return 0;
}

// callback that starts it all is the read callback
int poServer_echoRequest(struct POServer_connection *conn, void *data)
{
    static __thread int rcount = 0;
    int fd;
    fd = conn->fd;
    DASSERT(fd >= 0);
    struct Buf *buffer;
    char *buf;
    size_t n = 0;
    int ret = 0;
    errno = 0;


    if(data == NULL)
    {
        if(ASSERT(buffer = malloc(sizeof(*buffer))))
            return -1; // fail
        poServer_setCleanupCallback(conn,
                (POServer_callback_t) echoCleanup, buffer);
    }
    else
        buffer = data;

    buf = buffer->buf;

    while(n < MAX_READ)
    {
        ssize_t rt;

        rt = read(fd, &buf[n], MAX_READ - n);
        
        if(rt > 0)
        {
            n += rt;
            break;
        }
        if(rt == -1)
        {
            DLOG("read(fd=%d,,) failed\n", conn->fd);

            switch(errno)
            {
                case EWOULDBLOCK:
                    // This is how we know we are done with this non-blocking
                    // read.  We'll be back to get more later.
                    if(n == 0)
                        return 0; // nothing to spew
                    break;
                case EINTR: //interrupted by a signal
                    continue; // try read() again
                    break;
                case EPIPE: // "Broken pipe"
                case ECONNRESET: // "Connection reset by peer"
                    ret = 1; // error, but not too bad.
                    break; // there may be something to spew
                default:
                    VASSERT(0, "write(fd=%d,,) failed "
                            "with unhandled errno\n", conn->fd);
                    // Unexpected error, write more code here.
                    return -1;
            }
        }
        if(rt == 0)
        {
            // This fd is to be closed even if n != 0
            ret = 1;
            break;
        }
    }

    // Spew for the fun of it.

    buf[n] = '\0';
    printf("%d READ(fd=%d,%zu): %s\n", ++rcount, fd, n, buf);

    if(ret)
        return ret;

    buffer->len = n;
    buffer->w = 0;

    // remove read callback for now.  We may put it back in
    // the write callback.
    poServer_setReadCallback(conn, NULL, NULL);

    DASSERT(conn->writeCallback == NULL);
    // This will call the write callback when the socket is
    // ready for writing.
    return poServer_setWriteCallback(conn,
            (POServer_callback_t) echoWrite, buffer);
}
