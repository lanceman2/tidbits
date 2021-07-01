#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/file.h>
#include <stdarg.h>

#include "service.h"


struct Out
{
    enum OUTTYPE
    {
        OUTBUFFER,
        OUTPATH
    } type;
    
    struct Out *next; // for queue
};

struct OutPathTls
{
    struct Out h; // header
    int fd;

    uint8_t buffer[1024];
    size_t i,        // index out of buffer
           len,      // buffer fill length
           fileLeft; // amount left in file to read
#ifdef POSERVICE_FLOCK
    bool haveFLock;
#endif
};

struct OutPath
{
    struct Out h; // header
    int fd;
#ifdef POSERVICE_FLOCK
    bool haveFLock;
#endif
};

struct OutBuffer
{
    struct Out h; // header

    uint8_t *buffer; // user buffer passed in
    size_t index, len;
};

// create and add to the last in the output queue
static inline
void *outQueuePush(struct POService_session *s,
        size_t size, enum OUTTYPE type)
{
    DASSERT(s);

    struct Out *out;
    if(ASSERT((out = malloc(size)) != NULL))
        return NULL;

    memset(out, 0, size);

    out->type = type;
    out->next = NULL;
 
    if(s->lastOut)
    {
        DASSERT(s->firstOut);
        s->lastOut->next = out;
        s->lastOut = out;
    }
    else
    {
        DASSERT(s->firstOut == NULL);
        s->firstOut = s->lastOut = out;
    }
    return (void *) out;
}

// pop the first off the output queue, it must be freed at some time
static inline
struct Out *outQueuePop(struct POService_session *s)
{
    DASSERT(s);
    if(s->firstOut == NULL)
        return NULL;

    struct Out *out;
    out = s->firstOut;
    s->firstOut = s->firstOut->next;
    if(s->firstOut == NULL)
        s->lastOut = NULL;
    return out;
}

static inline
void freeOut(struct Out *out)
{
    DASSERT(out);
    if(out->type == OUTBUFFER)
    {
        DASSERT(((struct OutBuffer *) out)->buffer);
        free(((struct OutBuffer *) out)->buffer);
    }
    else
    {
        DASSERT(out->type == OUTPATH);
        if(((struct OutPath *) out)->fd != -1)
            close(((struct OutPath *) out)->fd);
    }
    free(out);
}

// POServer TCP cleanup callback
static int
Cleanup(struct POServer_connection *conn, struct POService_session *s)
{
    // We do not need to cleanup the server connections here,
    // that's done after this.
    DASSERT(conn);
    DASSERT(s);
    DASSERT(s->inBuffer);
    DASSERT(s->inSize);
    DLOG("\n");

    int ret = 0;

    if(s->cleanup)
    {
        DLOG("calling user session cleanup callback\n");
        s->cleanup(s, s->cleanupData);
    }

    if(s->tlsSession)
    {
        if(poTlsSession_destroy(s->tlsSession))
            ret = -1;
        DLOG("cleaned up TLS session\n");
    }

    free(s->inBuffer);

    while(s->firstOut)
        freeOut(outQueuePop(s));

#ifdef PO_DEBUG
    memset(s, 0, sizeof(*s));
#endif

    free(s);
    return ret;
}

static int
Write(struct POServer_connection *conn, struct POService_session *s);

// returns 2 if we exhausted the socket for writing and we'll be back
// returns 0 if we finished with success
// returns 1 if we need to close the socket
// returns -1 if we need to close the socket with error
static inline
int TLSWrite_buffer(gnutls_session_t s, struct OutBuffer *b)
{
    DASSERT(b->len > b->index);
    while(true)
    {
        ssize_t rt;
        rt = gnutls_record_send(s, &b->buffer[b->index],
                b->len - b->index);

        if(rt > 0)
        {
            if(rt == b->len - b->index);
                // Most likely case.
                return 0; // success

            DASSERT(rt < b->len - b->index);
            // For stream-oriented files, like this TCP, if we
            // write less than requested, like so, we are done
            // with the current epoll event.
            b->index += rt;
            return 2; // We'll be back
        }
        else if(rt < 0)
        {
            DLOG("gnutls_record_send(,,) failed err=%zd \"%s\"\n",
                    rt,  gnutls_strerror(rt));

            switch(rt)
            {
                case GNUTLS_E_AGAIN:
                    // The epoll event really knows it's exhausted now.
                    return 2; // We'll be back.
                case GNUTLS_E_INTERRUPTED: //interrupted by a signal
                    // Try again.
                    // This is the only looping case.
                    continue;
                default:
                    WLOG("gnutls_record_send(,,) failed err=%zd \"%s\"\n",
                            rt,  gnutls_strerror(rt));
                    // TODO: Unexpected error, write more code here.
                    return -1;
            }
        }

        // rt == 0 end of write session, somehow it closed
        return 1; // the socket is done and needs to be closed
    }
}

// returns 2 if we exhausted the socket for writing and we'll be back
// returns 0 if we finished with success
// returns 1 if we need to close the socket
// returns -1 if we need to close the socket with error
static inline
int TCPWrite_buffer(int fd, struct OutBuffer *b)
{
    DASSERT(b->len > b->index);

    ssize_t rt;
    rt = write(fd, &b->buffer[b->index], b->len - b->index);

    if(rt > 0)
    {
        if(rt == b->len - b->index)
            // most likely case.
            return 0; // done, with success

        DASSERT(rt < b->len - b->index);
        // For stream-oriented files, like this TCP, if we write less
        // than requested, like so, we are done with the current epoll
        // event.
        b->index += rt;
        return 2; // Arnold says: "I'll be back."
    }
    else if(rt < 0)
    {
        DLOG("write(fd=%d,,) failed\n", fd);
           
        switch(errno)
        {
            case EAGAIN:
                // The epoll event really knows it's exhausted now.
                return 2; // We'll be back.
            case EINTR: //interrupted by a signal
                // This is the only looping case and not a very likely one
                // either.  Will we blow the stack?  Only if there is a
                // jammed signal and write blocks when it should not.
                // What's better this or looping?
                return TCPWrite_buffer(fd, b);
            case EPIPE: // "Broken pipe"
            case ECONNRESET: // "Connection reset by peer"
                // This is not an error from this program.  Someone pulled
                // the plug.
                return 1; // fd needs to be closed
            default:
                VASSERT(0, "write(fd=%d,,) failed "
                        "with unhandled errno\n", fd);
                // Unexpected error, write more code here.
                return -1;
        }
    }
        
    //else // rt == 0 end of read session
    return 1; // the fd is done and needs to be closed
}

// returns 2 if we exhausted the socket for writing and we'll be back
// returns 0 if we finished with success
// returns 1 if we need to close the socket
// returns -1 if we need to close the socket with error
//
// The kernel will not encrypt through sendfile() like with the netflix
// freeBSD kernel, so we use a buffer to send a file with TLS.
static inline
int TLSWrite_path(gnutls_session_t s, struct OutPathTls *p)
{
    DASSERT(p);
    DASSERT(p->fd >= 0);

    if(!p->fileLeft)
    {
#ifdef POSERVICE_FLOCK
        // We have a file descriptor open for reading so the file should
        // exist at least until we close it.
        if(!p->haveFLock)
            while(flock(p->fd, LOCK_SH) != 0)
            {
                if(errno != EINTR)
                {
                    ELOG("flock(fd=%d,LOCK_SH) failed\n", p->fd);
                    return -1; // Screw this connection.
                }
            }
        // closing the file with release the advisory lock

        // The advisory lock is to make fstat() and sendfile() calls be
        // like atomic calls, overcoming the race condition of having the
        // file size change between these two system calls.
        //
        // We keep this lock until this file is written.  Is that a good
        // idea?
#endif
        struct stat st;
        if(ASSERT(fstat(p->fd, &st) == 0) || st.st_size == 0)
            // Yup we are screwed.
            return -1; // error will close the file and socket
        p->fileLeft = st.st_size;
    }


    // while we have data unread in file or data in buffer to read
    while(p->fileLeft || p->i < p->len)
    {
        ssize_t r;

        // while buffer is not full and there is more to read
        while(p->len < sizeof(p->buffer) && p->fileLeft)
        {
            size_t rlen;
            rlen = sizeof(p->buffer) - p->len;
            if(rlen > p->fileLeft) rlen = p->fileLeft;

            r = read(p->fd, &p->buffer[p->len], rlen);

            if(r > 0)
            {
                p->len += r;
                p->fileLeft -= r;
            }
            else
            {
                DLOG("read(,,)=%zd failed\n", r);

                switch(errno)
                {
                    case EINTR:
                        continue;
                    default:
                        WLOG("read(,,)=%zd failed\n", r);
                        return -1;
                }
            }
        }

        DASSERT(p->i < p->len);

        r = gnutls_record_send(s, &p->buffer[p->i], p->len - p->i);

        if(r > 0)
        {
            if(r == p->len - p->i)
            {
                p->i = 0;
                p->len = 0;

                if(p->fileLeft)
                    continue;
                return 0; // success, done
            }
            DASSERT(r < p->len - p->i);
            p->i += r;
            return 2; // I'll be back.
        }

        if(r == 0)
            // What's more common this or the error cases below.
            return 1; // The socket needs to be closed.

        // Fall to the error case here lastly

        DASSERT(r < 0);

        DLOG("gnutls_record_send(,,) failed err=%zd \"%s\"\n",
                        r,  gnutls_strerror(r));
            
        switch(r)
        {
            case GNUTLS_E_AGAIN:
                // The epoll event really knows it's exhausted now.
                return 2; // We'll be back.
            case GNUTLS_E_INTERRUPTED: //interrupted by a signal
                // Try again.  This is another looping case.
                continue;
            default:
                WLOG("gnutls_record_send(,,) failed err=%zd \"%s\"\n",
                        r,  gnutls_strerror(r));
                // TODO: Unexpected error, write more code here.
                return -1;
        }
    }

    // We never get here but this keeps GCC from complaining.
    return 0; // success, done
}

// returns 2 if we exhausted the socket for writing and we'll be back
// returns 0 if we finished with success
// returns 1 if we need to close the socket
// returns -1 if we need to close the socket with error
static inline
int TCPWrite_path(int fd, struct OutPath *p)
{
    DASSERT(p);
    DASSERT(fd >= 0);

#ifdef POSERVICE_FLOCK
    // We have a file descriptor open for reading so the
    // file should exist at least until we close it.
    if(!p->haveFLock)
        while(flock(p->fd, LOCK_SH) != 0)
        {
            if(errno != EINTR)
            {
                ELOG("flock(fd=%d,LOCK_SH) failed\n", p->fd);
                return -1; // Screw this connection.
            }
        }
    // closing the file with release the advisory lock

    // The advisory lock is to make fstat() and sendfile() calls
    // be like atomic calls, overcoming the race condition of having
    // the file size change between these two system calls.
#endif

    struct stat st;
    if(ASSERT(fstat(p->fd, &st) == 0) || st.st_size == 0)
        // Yup we are screwed.
        return -1; // error

    ssize_t rt;
    // TODO: consider socket options TCP_CORK and TCP_NODELAY ??
    rt = sendfile(fd, p->fd, NULL, st.st_size);

    if(rt == st.st_size)
        // Most likely case.
        return 0; // success

    if(rt < 0)
    {
        DLOG("sendfile(,,) failed to send\n");
        switch(errno)
        {
            case EAGAIN:
                return 2; // We'll be back.
            case EINTR:
                // Try again.  Will this loop infinity and
                // blow the stack?
                return TCPWrite_path(fd, p);
            case EPIPE: // "Broken pipe"
            case ECONNRESET: // "Connection reset by peer"
                // This is not an error from this program.  Someone pulled
                // the plug.
                return 1; // fd needs to be closed
            default:
                // TODO: call user error handler?
                WLOG("sendfile(,,) failed to send\n");
                return -1;
        }
    }

    DASSERT(rt < st.st_size);
    WLOG("sendfile(,,%zd) sent %jd of %jd bytes\n",
            rt,(intmax_t)st.st_size, (intmax_t)st.st_size);
    return -1; // error close the socket connection
}

// POService master server write callback
//
// Goes through the queue of requested write actions calling the
// appropriate socket write function ...
static int
Write(struct POServer_connection *conn, struct POService_session *s)
{
    DASSERT(conn);
    DASSERT(s);
    DASSERT(s->firstOut);
    int ret;

    while(true)
    {
        if(s->firstOut->type == OUTBUFFER)
        {
            if(s->tlsSession)
                ret = TLSWrite_buffer(s->tlsSession,
                        (struct OutBuffer *) s->firstOut);
            else
                ret = TCPWrite_buffer(conn->fd,
                        (struct OutBuffer *) s->firstOut);
        }
        else
        {
            DASSERT(s->firstOut->type == OUTPATH);

            if(s->tlsSession)
                ret = TLSWrite_path(s->tlsSession,
                        (struct OutPathTls *) s->firstOut);
            else
                ret = TCPWrite_path(conn->fd,
                        (struct OutPath *) s->firstOut);
        }

        // Now look for what to do next.


        if(ret && ret != 2)
            // close the socket connection, cleanup will be called
            return ret;

        if(ret == 2)
            // The epoll event was exhausted
            return 0; // we'll be back doing same request again

        DASSERT(ret == 0); // successfully done with write request

        // pop and clean off the finished request in the queue
        freeOut(outQueuePop(s));

        // go to next in write queue if there is any
        if(!s->firstOut)
            break;
    
    } // while(true)


    // There are no move user write requests and the epoll event was NOT
    // exhausted, so we must remove the write callback.  If we get more
    // write requests then the callback will be reinstated and called. 
    poServer_setWriteCallback(conn, NULL, NULL);

    // If there is no read callback this will terminate the
    // connection/session and the service of this socket connection would
    // be done.

    if(conn->readCallback)
        return 0; // keep connection
    return 1; // close connection
}

// returns 2 for new data read
static inline int
TLSRead(gnutls_session_t s, uint8_t **buffer, size_t *inSize,
        size_t *inLen, size_t maxMessageLength)
{
    uint8_t *buf;
    buf = *buffer;
#ifdef PO_DEBUG
    size_t oldLen;
    oldLen = *inLen;
#endif

#if 0
    // This gnutls_record_check_pending() shit is broken.
    size_t r;
    if(!(r = gnutls_record_check_pending(s)))
    {
        ELOG("NO TLS data pending  gnutls_record_check_pending()=%zu\n", r);
    }
#endif

    while(true)
    {
        ssize_t rt;
        rt = gnutls_record_recv(s, &buf[*inLen], *inSize - *inLen);

        if(rt > 0)
        {
             *inLen += rt;

            if(*inSize > *inLen)
            {
                // For stream-oriented files, like this TCP, if we read
                // less than requested, like so, we are done with the
                // current epoll event.  Maybe we'll get more here from
                // another epoll event.
                break;
            }

            DASSERT(*inLen == *inSize);
            
            if(*inSize < maxMessageLength)
            {
                // This is the most questionable part of all this
                // code.  realloc() come on now...
                //
                // add one byte so we may add a '\0' after reading
                if(ASSERT((buf = *buffer = realloc(buf,
                        (*inSize += PO_SERVICE_CHUNKLEN) + 1)) != NULL))
                    return -1; // close connection with error
            }
            else
            {
                WLOG("request message too large (>%zu bytes)"
                        " dropping connection\n",
                        *inSize);
                return 1; // close this connection
            }

            // now read some more
        }
        else if(rt < 0)
        {
            DLOG("gnutls_record_recv(,,) failed err=%zd \"%s\"\n",
                    rt, gnutls_strerror(rt));

            switch(rt)
            {
                case GNUTLS_E_AGAIN:
                    // This can happen even when no data was read.
                    // Done reading for now.
                    return 2;
                case GNUTLS_E_INTERRUPTED: //interrupted by a signal
                    continue; // try gnutls_record_recv() again
                default:
                    WLOG("gnutls_record_recv(,,) failed "
                        "%sfatal error=%zd \"%s\"\n",
                        gnutls_error_is_fatal(rt)?"":"non-", 
                        rt, gnutls_strerror(rt));
                    return 1; // close
            }
        }
        else // rt == 0
            return 1; // closing time
    }

    // We have more data
    DASSERT(oldLen < *inLen);
    return 2;
}

// returns 2 for new data read
static inline int
TCPRead(int fd, uint8_t **buffer, size_t *inSize, size_t *inLen,
        size_t maxMessageLength)
{
    uint8_t *buf;
    buf = *buffer;
#ifdef PO_DEBUG
    size_t oldLen;
    oldLen = *inLen;
#endif

    while(true)
    {
        ssize_t rt;
        rt = read(fd, &buf[*inLen], *inSize - *inLen);

        if(rt > 0)
        {
            *inLen += rt;

            if(*inSize > *inLen)
            {
                // For stream-oriented files, like this TCP, if we read
                // less than requested, like so, we are done with the
                // current epoll event.  Maybe we'll get more here from
                // another epoll event.
                break;
            }

            DASSERT(*inLen == *inSize);
            
            if(*inSize < maxMessageLength)
            {
                // This is the most questionable part of all this
                // code.  realloc() come on now...
                //
                // add one byte so we may add a '\0' after reading
                if(ASSERT((buf = *buffer = realloc(buf,
                        (*inSize += PO_SERVICE_CHUNKLEN) + 1)) != NULL))
                    return -1; // close connection with error
            }
            else
            {
                WLOG("request message too large (>%zu bytes)"
                        " dropping connection fd=%d\n",
                        *inSize, fd);
                return 1; // close this connection
            }

            // now read some more
        }
        else if(rt < 0)
        {
            DLOG("read(fd=%d,,) failed\n", fd);

            switch(errno)
            {
                case EAGAIN:
                    // This can happen even when no data was read.
                    // Done reading for now.
                    return 2;
                case EINTR: //interrupted by a signal
                    continue; // try read() again
                case EPIPE: // "Broken pipe"
                case ECONNRESET: // "Connection reset by peer"
                    return 1; // close this connection
                default:
                    VASSERT(0, "read(fd=%d,,) failed unexpected errno\n",
                            fd);
                    // TODO: Unexpected error, write more code here.
                    return -1;
            }
        }
        else // rt == 0
            return 1; // closing time
    }

    // We have more data
    DASSERT(oldLen < *inLen);
    return 2;
}

// POServer TCP, with or without TLS, read callback
static int
Read(struct POServer_connection *conn, struct POService_session *s)
{
    DASSERT(conn);
    DASSERT(s);
    DASSERT(s->service);
    DASSERT(conn->fd >= 0);
    DASSERT(s->inBuffer);
    DASSERT(s->inSize > s->inLen);

    DLOG("\n");

    int r;
    if(s->tlsSession)
        r = TLSRead(s->tlsSession, &s->inBuffer, &s->inSize,
                &s->inLen, s->service->maxMessageLength);
    else
        r = TCPRead(conn->fd, &s->inBuffer, &s->inSize,
                &s->inLen, s->service->maxMessageLength);
    if(r != 2)
        return r;

    if(s->inLen < 1)
        // We read no new data, but we are still serving.
        return 0;

    // We have more data and we have exhausted the epoll read event.


    enum POServiceReturn rt;

    // call the users callback
    rt = s->sessionCallback(s, s->inBuffer,
            s->inLen, s->sessionCallbackData);

    switch(rt)
    {
        case POSERVICE_CONTINUE:
            // continue to work on this connection
            s->inLen = 0; // reset buffer
        case POSERVICE_WAIT:
            // message was not complete, we continue filling buffer
            return 0;
        case POSERVICE_CLOSE:
            if(s->firstOut == NULL)
            {
                DASSERT(conn->writeCallback == NULL);
                // sends are complete, we are done with session
                return 1; // signal call cleanup callback to server
            }
            // else wait for the writes to finish
            
            // There should be a server write callback
            DASSERT(conn->writeCallback == (POServer_callback_t) Write);
            // remove read callback from the server
            poServer_setReadCallback(conn, NULL, NULL);
            // The Write() callback will return the close code when it
            // finishes.
            return 0;
        case POSERVICE_ERROR:
            // This error case differs from close case above in that we do
            // not wait for writes to finish.
            return -1;
#ifdef PO_DEBUG
        default:
            // This should not happen.
            DVASSERT(0, "unhandled case %d\n", rt);
            return -1;
#endif
    }

    return 0;
}

// POServer (with or without TLS) setup callback
static int
Setup(struct POServer_connection *conn, struct POService *service)
{
    DASSERT(conn);
    DASSERT(service);

    struct POService_session *s;
    if(ASSERT((s = malloc(sizeof(*s))) != NULL))
        return -1;

    memset(s, 0, sizeof(*s));

    s->service = service;
    s->conn = conn;
    s->inSize = PO_SERVICE_CHUNKLEN;
    s->sessionCallback = service->callback;
    s->sessionCallbackData = service->callbackData;
    if(service->tls)
    {
        s->tlsSession = poTlsSession_create(service->tls, conn->fd);
        if(!s->tlsSession) return -1; // error
    }
    // add one byte so we may add a '\0' after reading
    if(ASSERT((s->inBuffer = malloc(s->inSize + 1)) != NULL))
        return -1;
    s->inLen = 0;

    DASSERT(conn->cleanupCallback == NULL);
    poServer_setCleanupCallback(conn, (POServer_callback_t) Cleanup, s);

    // We are in the current read callback, so poServer_setReadCallback()
    // will not call it when we set it here.
    DASSERT(conn->readCallback == (POServer_callback_t) Setup);

    poServer_setReadCallback(conn, (POServer_callback_t) Read, s);
    // Now we do the read callback.
    return Read(conn, s);
}

// free(buffer) will be called for the user.
bool poService_send(struct POService_session *s,
        void *buffer, size_t bytes)
{
    DASSERT(s);
    DASSERT(s->conn);
    DASSERT(s->conn->cleanupData);
    DASSERT(s == s->conn->cleanupData);

    struct OutBuffer *b;
    b = outQueuePush(s, sizeof(*b), OUTBUFFER); // allocate a OutBuffer
    b->buffer = buffer; // buffer from user
    b->len = bytes;
    b->index = 0;

    if(s->conn->writeCallback == NULL)
    {
        // This must be the first Out thingy in the session queue
        DASSERT(b == (struct OutBuffer *) s->firstOut);
        // There are no other write callbacks waiting
        // This will call Write() now if the socket is ready for writing
        // or it will set it up to call later when the socket is ready for
        // writing.
        return poServer_setWriteCallback(s->conn,
                (POServer_callback_t) Write, s);
    }
#ifdef PO_DEBUG
    else
        DASSERT(s->conn->writeCallback == (POServer_callback_t) Write);
#endif

    return false;
}

bool poService_sendF(struct POService_session *s, const char *fmt, ...)
{
    DASSERT(s);
    DASSERT(s->conn);
    DASSERT(s->conn->cleanupData);
    DASSERT(s == s->conn->cleanupData);

    int n = PO_SERVICE_CHUNKLEN - 1;
    va_list ap;
    size_t len;
    char *buffer = NULL;

#ifdef PO_DEBUG // We check the dumbest things when in debug mode.
    int loopCount = 0;
#endif

    do
    {
        if(buffer) free(buffer);
        // TODO: Is this better than realloc() given we do
        // not need to save the previous data written.
        if(ASSERT(buffer = malloc(len = n + 1)))
            return true; // error

        va_start(ap, fmt);
        n = vsnprintf(buffer, len, fmt, ap);
        va_end(ap);
    }
    while(n >= len
#ifdef PO_DEBUG
            && !DASSERT(++loopCount < 2)
#endif
            );

    DASSERT(len > 1);
    DASSERT(n > 0);

    struct OutBuffer *b;
    b = outQueuePush(s, sizeof(*b), OUTBUFFER); // allocate a OutBuffer
    b->len = n; // not writing the '\0' terminator.

DLOG("writing %d bytes buffer=\"%s\"\n", n, buffer);
    b->buffer = (uint8_t *) buffer;

    if(s->conn->writeCallback == NULL)
    {
        // This must be the first Out thingy in the session queue
        DASSERT(b == (struct OutBuffer *) s->firstOut);
        // There are no other write callbacks waiting
        // This will call Write() now if the socket is ready for writing
        // or it will set it up to call later when the socket is ready for
        // writing.
        return poServer_setWriteCallback(s->conn,
                (POServer_callback_t) Write, s);
    }
#ifdef PO_DEBUG
    else
        DASSERT(s->conn->writeCallback == (POServer_callback_t) Write);
#endif

    return false;

}

bool poService_sendFD(struct POService_session *s, int fd, bool haveFLock)
{
    DASSERT(s);
    DASSERT(fd >= 0);
    DASSERT(s->conn);
    DASSERT(s->conn->cleanupData);
    DASSERT(s == s->conn->cleanupData);

    if(s->tlsSession)
    {
        struct OutPathTls *p;
        p = outQueuePush(s, sizeof(*p), OUTPATH); // allocate a Out
        p->fd = fd;

#ifdef POSERVICE_FLOCK
        p->haveFLock = haveFLock;
#endif
    }
    else // not TLS
    {
        struct OutPath *p;
        p = outQueuePush(s, sizeof(*p), OUTPATH); // allocate a Out
        p->fd = fd;
#ifdef POSERVICE_FLOCK
        p->haveFLock = haveFLock;
#endif
    }

    // We have the file open and now we wait of the socket to be ready to
    // write to via epoll_wait() with the poServer thingy.

    if(s->conn->writeCallback == NULL)
    {
        // There are no other write callbacks waiting
        // This will call Write() now if the socket is ready for writing
        // or it will set it up to call later when the socket is ready for
        // writing.
        return poServer_setWriteCallback(s->conn,
                (POServer_callback_t) Write, s);
    }
#ifdef PO_DEBUG
    else
        DASSERT(s->conn->writeCallback == (POServer_callback_t) Write);
#endif

    return false;
}


bool poService_sendPath(struct POService_session *s, const char *path)
{
    DASSERT(s);
    DASSERT(s->conn);
    DASSERT(s->conn->cleanupData);
    DASSERT(s == s->conn->cleanupData);
    int fd;
    fd = open(path, O_RDONLY);
    if(fd == -1)
    {
        WLOG("open(\"%s\", O_RDONLY) failed\n", path);
        return true; // fail, error
    }

    if(s->tlsSession)
    {
        struct OutPathTls *p;
        p = outQueuePush(s, sizeof(*p), OUTPATH); // allocate a Out
        p->fd = fd;
        p->i = 0; // we use a buffer to send a file with TLS
        p->len = 0;   // because we must send it encrypted
        p->fileLeft = 0;
    }
    else // not TLS
    {
        struct OutPath *p;
        p = outQueuePush(s, sizeof(*p), OUTPATH); // allocate a Out
        p->fd = fd;
    }

    // We have the file open and now we wait of the socket to be ready to
    // write to via epoll_wait() with the poServer thingy.

    if(s->conn->writeCallback == NULL)
    {
        // There are no other write callbacks waiting
        // This will call Write() now if the socket is ready for writing
        // or it will set it up to call later when the socket is ready for
        // writing.
        return poServer_setWriteCallback(s->conn,
                (POServer_callback_t) Write, s);
    }
#ifdef PO_DEBUG
    else
        DASSERT(s->conn->writeCallback == (POServer_callback_t) Write);
#endif

    return false;
}

// just TCP
bool poService_create(struct POService *s,
        struct POServer *server,
        struct POTls *tls,
        const char *port, const char *address,
        size_t maxMessageLength,
        POService_requestCallback_t callback,
        void *data)
{
    DASSERT(s);
    DASSERT(server);
    DASSERT(callback);
    DASSERT(server->efd >= 0);
    DASSERT(maxMessageLength >= 2);
    s->tls = tls;
    s->callback = callback;
    s->callbackData = data;
    s->maxMessageLength = maxMessageLength;
    if(poServer_listenerSetup(server, port, address,
                (POServer_callback_t) Setup, s))
        return true; // error

    return false;
}

void
poService_setRead(struct POService_session *s,
        POService_requestCallback_t callback, void *data)
{
    DASSERT(s);
    if(callback)
    {
        s->sessionCallback = callback;
        s->sessionCallbackData = data;
    }
    else
    {
        // reset to the starting callback and data
        s->sessionCallback = s->service->callback;
        DASSERT(!data);
        s->sessionCallbackData = s->service->callbackData;
    }
}

void
poService_setCleanup(struct POService_session *s,
        POService_cleanupCallback_t callback, void *data)
{
    DASSERT(s);
    // callback can be NULL to unset it.
    s->cleanup = callback;
    s->cleanupData = data;
}
