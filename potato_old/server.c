#include <sys/types.h>
#include <float.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h> // IPPROTO_TCP
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h> // printf()
#include <inttypes.h>
#include <time.h>
#include <limits.h>
//#include <sched.h> // sched_yield()

#include "server.h"
#include "util.h"



//////////////////////////////////////////////////////////////
//         Stuff used only here from the potato class
//////////////////////////////////////////////////////////////
extern
bool _poTato_init(void);
extern
void _poTato_addServer(struct POServer *s);
extern
void _poTato_removeServer(struct POServer *s);
extern
int _poTato_findListenerFd(struct sockaddr *addr);
extern
void _poTato_handoffCleanup(void);
extern
char *_poTato_getAddrStr(const struct sockaddr *addr,
        char *buf, size_t len);
extern
void _poTato_checkSignalNewServerContinue(void);
//////////////////////////////////////////////////////////////




// pull the first out of the queue
static inline
void queuePull(struct POServer_listenerWaitingQueue *q)
{
    DASSERT(q);
    DASSERT(q->first && q->last);

    q->first->waiting = false; // no longer waiting

    q->first = q->first->next;
    if(!q->first)
        q->last = NULL;
}

// push to the last in the queue
static inline
void queuePush(struct POServer_listenerWaitingQueue *q,
        struct POServer_listener *l)
{
    DASSERT(q);
    DASSERT((q->first && q->last) || (!q->first && !q->last));

    if(l->waiting) return; // it's waiting already
    
    l->next = NULL;
    l->waiting = true;

    if(q->last)
    {
        q->last->next = l;
        q->last = l;
    }
    else
    {
        q->first = l;
        q->last = l;
    }
}

// pop a connection from the front of the unused queue
static inline
struct POServer_connection *unusedPop(
        struct POServer_connectionQueue *queue)
{
    DASSERT(queue);

    if(!queue->front)
    {
        DASSERT(!queue->back);
        return NULL;
    }

    struct POServer_connection *conn;
    conn = queue->front;
    queue->front = queue->front->next;
    if(!queue->front)
        queue->back = NULL;
    return conn;
}

// push a connection to the back of the unused queue
static inline
void unusedPush(struct POServer_connectionQueue *queue,
        struct POServer_connection *conn)
{
    DASSERT(queue);
    DASSERT(conn);

    conn->next = NULL;

    if(queue->back)
    {
        DASSERT(queue->front);
        queue->back->next = conn;
    }
    else
    {
        DASSERT(!queue->front);
        queue->front = conn;
    }

    queue->back = conn;
}

// pops a connection off of unused queue and add it to
// last in used list and returns it.
static inline
struct POServer_connection *listAddNew(struct POServer *s)
{
    DASSERT(s);
    DASSERT(s->used.num < PO_SERVER_MAX_CONNECTIONS);

    struct POServer_connection *conn;
    struct POServer_connectionList *l;
    mutexLock(&s->unused.front->mutex);
    conn = unusedPop(&s->unused);

    DASSERT(conn);
    l = &s->used;

    DASSERT((l->first && l->last) || (!l->first && !l->last));
    DASSERT((l->first && l->last) || l->num == 0);
    DASSERT((!l->first && !l->last) || l->num > 0);
    DASSERT(!l->first || l->first->prev == NULL);
    DASSERT(!l->last || l->last->next == NULL);

#if 1
#ifdef PO_DEBUG
    // confirm that conn is NOT in the list
    struct POServer_connection *c;
    uint32_t len;
    // forward
    for(len=0,c=l->first;c;++len,c=c->next)
        if(c == conn)
            break;
    DASSERT(c != conn);
    DASSERT(len == l->num);
    // backward
    for(len=0,c=l->last;c;++len,c=c->prev)
        if(c == conn)
            break;
    DASSERT(c != conn);
    DASSERT(len == l->num);
#endif
#endif


#ifdef PO_SERVER_COUNT_CONNECTIONS
    ++l->num;
#endif

    if(l->last)
    {
        l->last->next = conn;
        conn->next = NULL;
        conn->prev = l->last;
        l->last = conn;
    }
    else
    {
        l->first = l->last = conn;
        conn->next = conn->prev = NULL;
    }

    return conn;
}

// remove from used list and push it to unused
static inline
void listRemove(struct POServer *s,
        struct POServer_connection *conn)
{
    DASSERT(s);
    DASSERT(conn);
    DASSERT(s->used.num > 0);
    DASSERT(s->used.num <= PO_SERVER_MAX_CONNECTIONS);

    struct POServer_connectionList *l;
    l = &s->used;

    DASSERT(l->first && l->last);
    DASSERT(l->first->prev == NULL);
    DASSERT(l->last->next == NULL);

#if 1
#ifdef PO_DEBUG
    // confirm that conn is in the list
    struct POServer_connection *c;
    // forward
    for(c=l->first;c;c=c->next)
        if(c == conn)
            break;
    DASSERT(c == conn);
    // backward
    for(c=l->last;c;c=c->prev)
        if(c == conn)
            break;
    DASSERT(c == conn);
#endif
#endif


#ifdef PO_SERVER_COUNT_CONNECTIONS
    --l->num;
#endif

    if(conn->next)
        conn->next->prev = conn->prev;
    else
    {
        DASSERT(conn == l->last);
        l->last = conn->prev;
    }

    if(conn->prev)
        conn->prev->next = conn->next;
    else
    {
        DASSERT(conn == l->first);
        l->first = conn->next;
    }
    unusedPush(&s->unused, conn);
}

bool _poServer_addListenerToEpoll(const struct POServer *s,
        const struct POServer_listener *l, int fd)
{
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = (void *) l;

    if(ASSERT(epoll_ctl(s->efd, EPOLL_CTL_ADD, fd, &ev) != -1))
        return true; // error

    return false; // success
}

// Makes a listening fd and adds it to the POServer epoll thingy
bool poServer_listenerSetup(struct POServer *s,
        const char *port, const char *address,
        POServer_callback_t callback, void *data)
{
    DASSERT(s);
    DASSERT(callback);

    int fd = -1;
    struct POServer_listener *l;

    DASSERT(s->numListeners < PO_SERVER_NUM_LISTENERS);

    l = &s->listener[s->numListeners];

    DVASSERT(l->fd == -1, "listener at index %d is already setup\n",
            s->numListeners);

    if(poUtil_setAddress(&l->addr, port, address, __func__))
        goto fail;

    errno = 0;

    // We first try to find the listening socket from the list gotten from
    // a handoff from an old running potato server from a call to
    // poTato_signalHandOff().
    if((fd = _poTato_findListenerFd(&l->addr)) == -1)
    {
        if(ASSERT((fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK,
                    IPPROTO_TCP)) >= 0))
            goto fail;

        int val = 1;
        if(ASSERT(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                    (void *) &val, sizeof(int)) == 0))
            goto fail;

        if(setNonblocking(fd))
            goto fail;

        if(ASSERT(bind(fd, &l->addr, sizeof(l->addr)) == 0))
            goto fail;
    }

    // Setting backlog waiting connection queue size
    if(ASSERT(listen(fd, PO_SERVER_LISTEN_BACKLOG) == 0))
        goto fail;

    if(_poServer_addListenerToEpoll(s, l, fd))
        goto fail;

    l->fd = fd;
    l->callback = callback;
    l->data = data;

#ifdef PO_ELOG_NOTICE
    char addr_buf[128];
    NLOG("listening fd=%d %s\n", fd, _poTato_getAddrStr(&l->addr,
                addr_buf, sizeof(addr_buf)));
#endif

    ++s->numListeners;


    if(s->numListeners == PO_SERVER_NUM_LISTENERS)
    {
        // We have all the listeners that we will ever have so now we can
        // close any extra listeners from the handoff.
        _poTato_handoffCleanup();
        DASSERT(s->isRunning == false);
        s->isRunning = true;
        WLOG("Server Ready and Listening for Requests on"
            " %"PRIu32" ports\n", s->numListeners);
    }

    return false; // success

fail:

    if(fd >= 0) close(fd);

    return true; // error
}

static inline
bool removeConnection(struct POServer *s, struct POServer_connection *conn)
{
    DASSERT(s);
    DASSERT(s->used.num <= PO_SERVER_MAX_CONNECTIONS);
    DASSERT(conn->fd >= 0);


    listRemove(s, conn);

    errno = 0; // set before calling close

    bool ret = false; // success

    if(conn->cleanupCallback)
        if(ASSERT(conn->cleanupCallback(conn, conn->cleanupData) == 0))
            ret = true; // error

#ifdef PO_SERVER_THREADPOOL
    // We make the accept time different
    ++conn->changeCount;
    conn->markedForRemoval = false;
#endif
    conn->cleanupCallback = conn->readCallback = 
        conn->writeCallback = NULL;
    conn->readReady = conn->writeReady = false;


    errno = 0;

#ifdef PO_SERVER_COUNT_CONNECTIONS
    DLOG("closing fd=%d there are now %d connections\n",
            conn->fd, s->used.num);
#else
    DLOG("closing fd=%d there %s left\n",
            conn->fd,
            s->used.first?"is at least one connection":
            "are no connections");
#endif


    if(ASSERT(close(conn->fd) == 0))
        // This is very bad!
        ret = true; // error

    conn->fd = -1;
    return ret;
}

// We must have the server mutex lock to call this.
static inline
bool checkTryRemoveConnection(struct POServer *s,
        struct POServer_connection *conn)
{
#ifdef PO_SERVER_THREADPOOL
    if(mutexTryLock(&conn->mutex))
    {
        removeConnection(s, conn);
        mutexUnlock(&conn->mutex);
        return true;
    }
    // We know that the worker thread will remove it because they
    // had the lock and are forced by the server mutex to look at
    // this flag.
    conn->markedForRemoval = true;
    return false;
#else
    removeConnection(s, conn);
    return true;
#endif
}

static inline
struct POServer_connection *addConnection(struct POServer *s,
        int fd, struct sockaddr_in *addr,
        const struct POServer_listener *l)
{
    DASSERT(s);
    DASSERT(addr);
    DASSERT(fd >= 0);
    DASSERT(s->used.num < PO_SERVER_MAX_CONNECTIONS);
    DASSERT(s->unused.back);

    if(setNonblocking(fd))
    {
        close(fd);
        return NULL; // error
    }
 
    // Pop unused connection off the unused queue
    struct POServer_connection *conn;
    conn = listAddNew(s);
    // after listAddNew(): we now have the conn->mutex lock

    conn->fd = fd;
    conn->readCallback = l->callback;
    conn->readData = l->data;
    conn->acceptTime = poTime_getDouble();

    struct epoll_event new_ev;
    new_ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
    // We make the user epoll data be the connection struct
    new_ev.data.ptr = conn;

    if(ASSERT(epoll_ctl(s->efd, EPOLL_CTL_ADD, fd, &new_ev) != -1))
    {
        // epoll_ctl() failed and we are screwed.
        removeConnection(s, conn);
        mutexUnlock(&conn->mutex);
        return NULL; // error
    }

#ifdef PO_SERVER_COUNT_CONNECTIONS
#  ifdef PO_ELOG_DEBUG
    char addr_buf[128];
    DLOG("added fd=%d %s:%d there are now %d connections%s\n", fd,
            inet_ntop(AF_INET, &addr->sin_addr, addr_buf,
            sizeof(addr_buf)), ntohs(addr->sin_port), s->used.num,
            (s->used.num == PO_SERVER_MAX_CONNECTIONS)?", the maximum":"");
#  else
#    ifdef PO_ELOG_INFO
    if(s->used.num == PO_SERVER_MAX_CONNECTIONS)
    {
        char addr_buf[128];
        ILOG("added fd=%d %s:%d there are now the maximum %d connections\n", fd,
                inet_ntop(AF_INET, &addr->sin_addr, addr_buf,
                sizeof(addr_buf)), ntohs(addr->sin_port), s->used.num);
    }
#    endif
#  endif
#endif
    mutexUnlock(&conn->mutex);
    return conn;
}

// The user passes in the memory to use for the object so it may be memory
// on the queue for higher performance by using less system resources.
struct POServer *poServer_create(struct POServer *s)
{
    int i;

    DASSERT(s);

    if(_poTato_init()) return NULL; // error

    memset(s, 0, sizeof(*s));

    // mark all fd as closed
    s->efd = -1;

    for(i=0;i<PO_SERVER_NUM_LISTENERS;++i)
        s->listener[i].fd = -1;

    for(i=0;i<PO_SERVER_MAX_CONNECTIONS;++i)
    {
        mutexInit(&s->conn[i].mutex);
        s->conn[i].fd = -1;
        s->conn[i].server = s;
        // Build the queue of unused connections from all in the
        // connection array.
        unusedPush(&s->unused, &s->conn[i]);
    }

#ifdef PO_SERVER_THREADPOOL
    poThreadPool_create(&s->threadPool);
    NLOG("Server running with threadpool with a maximum of %d threads\n",
        PO_THREADPOOL_MAX_NUM_THREADS);
#else
    NLOG("Server running with NO threadpool\n");
#endif


    mutexInit(&s->mutex);

    if(ASSERT((s->efd = epoll_create1(EPOLL_CLOEXEC)) >= 0))
        goto fail;


    _poTato_addServer(s);
  
    return s;

fail:

    if(s->efd >= 0) close(s->efd);

    return NULL;
}

// Must have server mutex lock before
static
bool _poServer_accept(struct POServer *s)
{
    DASSERT(s);
    DASSERT(s->used.num <= PO_SERVER_MAX_CONNECTIONS);
    DASSERT(s->unused.back);

    int conn_fd, fd;
    struct sockaddr_in addr;
    struct POServer_listener *l;
    socklen_t len;
    l = s->waiting.first;
    // There must be a waiting listener and this is the one first in the
    // listener waiting queue.
    DASSERT(l);
    DASSERT(l->fd >= 0);
    fd = l->fd;
    len = sizeof(addr);

    errno = 0;
    conn_fd = accept(fd, (struct sockaddr *) &addr, &len);

    if(conn_fd == -1 && errno == EAGAIN)
    {
        DLOG("accept(fd=%d,,) call got no connection\n", fd);
//ILOG("accept(fd=%d,,) call got no connection\n", fd);
        // reset waiting flag
        queuePull(&s->waiting);
        return false; // not an error
    }

    if(VASSERT(conn_fd >= 0, "accept(fd=%d,,) failed\n", fd))
    {
        return true; // error
    }

    if(addConnection(s, conn_fd, &addr, l))
    {
        return false; // success
    }

    return true; // error
}

#ifdef PO_SERVER_THREADPOOL
// Struct that is passed in the stack argument data of
// ServiceConnection() below.
struct POServer_worker
{
    struct POServer_connection *conn;
    // All this just is to add an int argument to the callback function
    // ServiceConnection().  The argument of this callback function must
    // be a pointer of some kind.  TODO: a more complex variable arg
    // interface to poThreadPool_runTask() might make this nicer here.
    int changeCount;
};

#ifdef PO_DEBUG
// TODO: remove this debug crap.
static inline
void checkStuff(struct POServer *s,
        struct POServer_connection *conn,
        struct POServer_worker *w)
{
    if(w->changeCount != conn->changeCount)
    {
        mutexLock(&s->mutex);
        ASSERT(0);
    }
}
#endif

static
void *ServiceConnection(struct POServer_worker *w)
#else // no threads
static inline // inline this function if there's no need for a pointer
// to a function for creating a worker thread.
void *ServiceConnection(struct POServer_connection *conn)
#endif
{
    struct POServer *s;

#ifdef PO_SERVER_THREADPOOL
    struct POServer_connection *conn;
    conn = w->conn;
    s = conn->server;

    // This mutex guarantees that only one thread works on this connection
    // at a time.  The service builder (user of the server class) has
    // complete control of what callbacks are running and in what order by
    // setting the current read and write callbacks for the connection
    // while in the server read and write callbacks.
    mutexLock(&conn->mutex);


DLOG("\n");

    if(w->changeCount != conn->changeCount)
    {
        // This connection was removed before we got the mutex lock, or it
        // was removed and than reused any number of times.  If it's a
        // reuse case another thread will be along later or has done this
        // already.  In any case this request is too old to be of use,
        // and the server is overloaded/miss-configured or we have a
        // timed-out connection I/O request.  Bla bla bla ...
        mutexUnlock(&conn->mutex);
        free(w);
        return NULL;
    }
#else
    // No need to check if async threads screwed us.
    s = conn->server;
#endif

#if 0
// TODO: remove this debug crap.
#  ifdef PO_DEBUG
#    define CHECK_CHANGE checkStuff(s, conn, w)
#  else
#    define CHECK_CHANGE  /*empty macro*/
#  endif
#else
// TODO: remove this debug crap.
#  define CHECK_CHANGE  /*empty macro*/
#endif
    
    DASSERT(conn->fd >= 0);
    
    bool removeMe = false;

    // Remember it is nonblocking I/O on the sockets.
    // Google it if you don't know that paradigm.

    if(conn->readReady && conn->readCallback)
    {
        CHECK_CHANGE;

        if(conn->readCallback(conn, conn->readData))
        {
            CHECK_CHANGE;
            removeMe = true;
            conn->readCallback = conn->writeCallback = NULL;
        }
        // At this point the callback may have removed itself
        else if(conn->readCallback)
        {
            CHECK_CHANGE;
            // We assume that the read event is exhausted
            // because the callback is still there ready to
            // get data.
            conn->readReady = false;
        }
        // else we are ready when the next read callback is set.
    }

    if(conn->writeReady && conn->writeCallback)
    {
        CHECK_CHANGE;
        
        if(conn->writeCallback(conn, conn->writeData))
        {
            CHECK_CHANGE;
            removeMe = true;
            conn->readCallback = conn->writeCallback = NULL;
        }
        // At this point the callback may have removed itself
        else if(conn->writeCallback)
        {
            CHECK_CHANGE;
            // We assume that the write event is exhausted
            // because the callback is still there ready to
            // send data.
            conn->writeReady = false;
        }
        // else we are ready when the next write callback is set.
    }

    if(!conn->readCallback && !conn->writeCallback)
        // This connection now has no input or output and there is no way
        // for it to get input or output without a callback.
        removeMe = true;

    mutexUnlock(&conn->mutex);

    // Reset mutexes so that we may remove the connection if it needs to
    // be removed, by getting the server mutex lock before the connection
    // mutex.  We could not keep the connection mutex lock, because the
    // master thread needs to do things like this too and it locks the
    // mutexes in this order.
#ifdef PO_SERVER_THREADPOOL
    mutexLock(&s->mutex);
    mutexLock(&conn->mutex);
    
    if((conn->changeCount == w->changeCount && removeMe)
            || conn->markedForRemoval)
        // We found that we needed to remove the connection in this
        // function and this is the same connection via conn->changeCount
        // == changeCountor OR the master thread tried to remove it but
        // could not get the connection mutex lock.
        removeConnection(s, conn);
    // else if(conn->changeCount == w->changeCount)
    //   we may be back calling ServiceConnection() with this same
    //   connection.
    //
    // else 
    //   the connection was removed by another worker thread or by the
    //   master server thread.

    mutexUnlock(&conn->mutex);
    mutexUnlock(&s->mutex);
    free(w);
#else
    // This is the reason not to use multiple threads, simple fast code
    // compared to above.
    if(removeMe)
        removeConnection(s, conn);
#endif

    return NULL;
}

// Must have server mutex lock before calling this.  Gets as many accept()
// connections as are asked for or until an existing worker thread would
// block getting it or we run out of timed-out connections.
#if PO_SERVER_CONNECTION_WAIT_TIMEOUT != -1
static inline
void lookForAcceptFromTimedOutConns(struct POServer *s, double t)
{
    // This will remove old, timed-out connections, if
    // checkTryRemoveConnection() gets a connection mutex lock(), if not
    // it will make the accept() after the "marked" connection is removed
    // by the worker thread.
    if(s->waiting.first && s->used.first)
    {
        // There is a listener in the wait queue
    
        DASSERT(s->used.num == PO_SERVER_MAX_CONNECTIONS);

#  if PO_SERVER_CONNECTION_LONG_TIMEOUT == -1
        if(t == DBL_MAX)
            t = poTime_getDouble();
#  endif

        while(s->used.first && s->waiting.first &&
            t - s->used.first->acceptTime >=
            PO_SERVER_CONNECTION_WAIT_TIMEOUT / 1000.0)
        {
            DLOG("marking for removal (%.2lf secs)"
                " oldest connection fd=%d\n",
                t - s->used.first->acceptTime, s->used.first->fd);
            if(checkTryRemoveConnection(s, s->used.first))
                _poServer_accept(s);
            else
                break; // we'll get it in the next loop
        }

#  ifdef PO_ELOG_NOTICE
        if(!s->unused.back && s->waiting.first)
            NLOG("https connections (%"PRIu32") maxed out waiting"
                    " to call accept()\n",
                PO_SERVER_MAX_CONNECTIONS);
#  endif
    }
}
#else // #if PO_SERVER_CONNECTION_WAIT_TIMEOUT != -1
#  define lookForAcceptFromTimedOutConns(x,y) /*empty macro*/
#endif // #if PO_SERVER_CONNECTION_WAIT_TIMEOUT != -1


// We must have a server mutex before calling this.
static void doEvent(struct POServer *s, struct epoll_event *ev)
{
    if(ev->data.ptr <= (void *) &s->listener[PO_SERVER_NUM_LISTENERS-1])
    {
        DASSERT(ev->events == EPOLLIN);
        // this is an accept() event
        struct POServer_listener *l;
        l = ev->data.ptr;

        if(!s->isRunning && l->fd == -1)
        {
            // We are shutting down.  This must be after a SIGUSR1 
            return;
        }
        DASSERT(l->fd >= 0);

        // accept() must be called until it fails with EAGAIN, so we say
        // that this listener is waiting for connections until the kernel
        // says we are not with errno == EAGAIN after calling accept().
        // Similar to non-blocking read().  queuePush() will not queue it
        // again.
        queuePush(&s->waiting, l);

        if(s->unused.back)
            // we have unused connection structs
            _poServer_accept(s);
        else
            lookForAcceptFromTimedOutConns(s, poTime_getDouble());
        return;
    }


    struct POServer_connection *conn;
    conn = ev->data.ptr;

#ifdef PO_SERVER_THREADPOOL
    if(conn->fd == -1)
        // This can happen if a worker thread removed the connection.
        return;
#endif

    // Should we need to check this, or is this an error case?
    if(!conn->readCallback && !conn->writeCallback)
    {
        checkTryRemoveConnection(s, conn);
        return;
    }

    if(ev->events & (EPOLLIN|EPOLLOUT))
    {
        bool callIt = false;

        // This will block until the last call (thread) with this
        // connection returns.
        mutexLock(&conn->mutex);
        if(ev->events & EPOLLIN)
            conn->readReady = true;
        if(ev->events & EPOLLOUT)
            conn->writeReady = true;
        if((conn->readReady && conn->readCallback)
                ||
            (conn->writeReady && conn->writeCallback))
            callIt = true;
        if(callIt)
#ifdef PO_SERVER_THREADPOOL
        {
            struct POServer_worker *w;
            // We must allocate argument passing data so that we have
            // values passed that so not change by another thread. The
            // callback ServiceConnection() will free it before returning.
            ASSERT(w = malloc(sizeof(*w)));
            w->changeCount = conn->changeCount;
            w->conn = conn;
            // This call can block if the threadPool task queue is full,
            // so we must NOT have a mutex lock when calling
            // poThreadPool_runTask().  A worker thread must be able to
            // get the server mutex lock in order to wake this master
            // thread here.
            mutexUnlock(&conn->mutex);
            mutexUnlock(&s->mutex);
            poThreadPool_runTask(&s->threadPool,
                    (void *(*)(void *)) ServiceConnection, w);
            mutexLock(&s->mutex);
        }
        else
            mutexUnlock(&conn->mutex);
#else
            ServiceConnection(conn);
#endif
    }

    // At this point a worker thread could have removed the connection.
    if(conn->fd == -1)
        return;

    if(ev->events & (EPOLLRDHUP|EPOLLERR))
    {
        DLOG("got%s%s for fd=%d\n",
                (ev->events & EPOLLRDHUP)?" EPOLLRDHUP":"",
                (ev->events & EPOLLERR)?" EPOLLERR":"",
                conn->fd);
        checkTryRemoveConnection(s, conn);
    }
}


#define LONG_TIMEOUT  (PO_SERVER_CONNECTION_LONG_TIMEOUT / 1000.0)

// We must have a server mutex lock before this call and it returns
// with the thread having the server mutex lock.
static
void _poServer_doNextEvent(struct POServer *s)
{
    errno = 0;

#if PO_SERVER_CONNECTION_LONG_TIMEOUT != -1 ||\
    PO_SERVER_CONNECTION_WAIT_TIMEOUT != -1
    double t = DBL_MAX;
#endif

#if PO_SERVER_CONNECTION_LONG_TIMEOUT != -1
    t = poTime_getDouble();

    // Remove very old connections.  No connection should take this long.
    while(s->used.first &&
        t - s->used.first->acceptTime >= LONG_TIMEOUT)
    {
        DASSERT(s->used.num > 0);
        DLOG("removing VERY old (%.2lf secs) connection fd=%d\n",
            t - s->used.first->acceptTime, s->used.first->fd);
        checkTryRemoveConnection(s, s->used.first);
    }
#endif

    while(s->waiting.first && s->unused.back)
        // We were waiting for unused connection slots to call accept()
        _poServer_accept(s);

    lookForAcceptFromTimedOutConns(s, t);

    // Next epoll_wait() for more events.
        
    DASSERT(PO_SERVER_EPOLL_WAIT_TIME >= -1);

    mutexUnlock(&s->mutex);

    // epoll_wait() is a blocking call, therefore we must unlock
    // the server mutex before calling it.

    int nfds;

    nfds = epoll_wait(s->efd, s->event, PO_SERVER_MAX_EPOLL_EVENTS,
            PO_SERVER_EPOLL_WAIT_TIME);

    DASSERT(nfds >= -1);

#if PO_SERVER_EPOLL_WAIT_TIME != -1
    if(nfds == 0)
    {
        //DLOG("epoll_wait() timed out\n");
#ifdef PO_SERVER_THREADPOOL
        poThreadPool_checkIdleThreadTimeout(&s->threadPool);
#endif
        mutexLock(&s->mutex);
        return; //success
    }
#endif

    if(nfds == -1 && errno == EINTR)
    {
        // a signal interrupted the epoll_wait() call
        NLOG("epoll_wait()\n");
        mutexLock(&s->mutex);
        return; // error, but not bad
    }

    if(VASSERT(nfds != -1, "epoll_wait() failed\n"))
    {
        mutexLock(&s->mutex);
        return; // error
    }

    DLOG("epoll_wait returned %d\n", nfds);

    mutexLock(&s->mutex);

    // Last: handle epoll events.
    struct epoll_event *ev;
    ev = s->event;
    while(nfds--)
        doEvent(s, ev++);
}

// public interface
void poServer_doNextEvents(struct POServer *s)
{
    DASSERT(s);
    DASSERT(s->efd >= 0);
    DASSERT(s->numListeners == PO_SERVER_NUM_LISTENERS);

    mutexLock(&s->mutex);
    _poServer_doNextEvent(s);
    mutexUnlock(&s->mutex);
}

// Here we close the listening sockets and wait for all the connections to
// time out or close on their own.  If there are no connection timeouts
// set and any connections hang on, then this will hang too, that's just
// how the user may configure the potato server, and it may be a value use
// case.
void poServer_shutdown(struct POServer *s)
{
    DASSERT(s);
    DASSERT(s->efd >= 0);
    DASSERT(s->numListeners == PO_SERVER_NUM_LISTENERS);

    // mutexLock(&s->mutex); // lock it later...
    s->listenersDown = true;
    // Tell new server to continue startup.  accept() on listening fd's
    // only come from the main thread which is here, so there is no race
    // condition for the order of things here.
    _poTato_checkSignalNewServerContinue();

    errno = 0; // sick of seeing errno=4:Interrupted system call
    DASSERT(s);
    uint32_t i;
    for(i=0; i<PO_SERVER_NUM_LISTENERS; ++i)
    {
        if(s->listener[i].fd >= 0)
        {
            close(s->listener[i].fd);
            s->listener[i].fd = -1;
            --s->numListeners;
        }
    }
    // Remove all the listening sockets, that are waiting, from the
    // waiting queue.
    while(s->waiting.first)
        queuePull(&s->waiting);

    DASSERT(s->numListeners == 0);

    mutexLock(&s->mutex);

#ifdef PO_SERVER_COUNT_CONNECTIONS
    ILOG("waiting for last %d connections to finish\n", s->used.num);
#else
    ILOG("waiting for last connections to finish\n");
#endif

    while(s->used.first)
        // while we have any used connections
        _poServer_doNextEvent(s);

    mutexUnlock(&s->mutex);
}

void poServer_destroy(struct POServer *s)
{
    DASSERT(s);
    DASSERT(s->efd >= 0);

    errno = 0;

    uint32_t i;
    for(i=0; i<PO_SERVER_NUM_LISTENERS; ++i)
    {
        if(s->listener[i].fd >= 0)
            close(s->listener[i].fd);
    }

#ifdef PO_SERVER_THREADPOOL
    // This waits for worker threads to finish.
    poThreadPool_destroy(&s->threadPool);
#endif

    // There are no other threads now so we don't need mutexes.

    while(s->used.first)
        removeConnection(s, s->used.first);

#ifdef PO_SERVER_THREADPOOL
    for(i=0;i<PO_SERVER_MAX_CONNECTIONS;++i)
        mutexDestroy(&s->conn[i].mutex);
#endif

    mutexDestroy(&s->mutex);
    
    DASSERT(s->used.num == 0);

    // TODO:  ??? shutdown(fd, SHUT_RDWR);

    for(i=0;i<PO_SERVER_NUM_LISTENERS;++i)
    {
        if(s->listener[i].fd >= 0)
            ASSERT(close(s->listener[i].fd) == 0);
    }

    ASSERT(close(s->efd) == 0);

    _poTato_removeServer(s);

#ifdef PO_DEBUG
    memset(s, 0, sizeof(*s));
#endif
    WLOG("Server Has Shutdown\n");
}


// to be used in connection callback with connection mutex locked
int poServer_setReadCallback(struct POServer_connection *conn,
        POServer_callback_t callback, void *data)
{
    DASSERT(conn);
    DASSERT(conn->fd >= 0);
    DASSERT(conn->server);

    POServer_callback_t oldCallback;
    oldCallback = conn->readCallback;

    conn->readCallback = callback;
    conn->readData = data;

    if(!oldCallback && callback && conn->readReady)
        return callback(conn, data);

    return 0; // success
}

// to be used in connection callback with connection mutex locked
int poServer_setWriteCallback(struct POServer_connection *conn,
        POServer_callback_t callback, void *data)
{
    DASSERT(conn);
    DASSERT(conn->fd >= 0);
    DASSERT(conn->server);

    POServer_callback_t oldCallback;
    oldCallback = conn->writeCallback;

    conn->writeCallback = callback;
    conn->writeData = data;

    if(!oldCallback && callback && conn->writeReady)
        return callback(conn, data);

    return 0; // success
}

// to be used in connection callback
void poServer_setCleanupCallback(struct POServer_connection *conn,
        POServer_callback_t callback, void *data)
{
    DASSERT(conn);
    DASSERT(conn->fd >= 0);
    DASSERT(conn->server);

    conn->cleanupCallback = callback;
    conn->cleanupData = data;
}
