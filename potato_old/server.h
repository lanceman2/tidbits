#ifndef __PO_SERVER_H__
#define __PO_SERVER_H__


#include <stdint.h>
#include <inttypes.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include "configure.h"

/* a server with no explicit heap memory allocation */

#ifndef PO_SERVER_NUM_LISTENERS
#  define PO_SERVER_NUM_LISTERS         2
#endif

#ifndef PO_SERVER_MAX_EPOLL_EVENTS
#  define PO_SERVER_MAX_EPOLL_EVENTS    10
#endif

#ifndef PO_SERVER_MAX_CONNECTIONS
#  define PO_SERVER_MAX_CONNECTIONS     10
#endif

#ifndef PO_SERVER_LISTEN_BACKLOG
#  define PO_SERVER_LISTEN_BACKLOG      20
#endif

#ifndef PO_SERVER_CONNECTION_WAIT_TIMEOUT
// time in milli seconds, 1 sec = 1000 milli seconds
#  define PO_SERVER_CONNECTION_WAIT_TIMEOUT     2000
#endif

#ifndef PO_SERVER_CONNECTION_LONG_TIMEOUT
// time in milli seconds, 1 sec = 1000 milli seconds
#  define PO_SERVER_CONNECTION_LONG_TIMEOUT     4000
#endif




#ifdef PO_DEBUG
// For debug we need to count connections
#  ifndef PO_SERVER_COUNT_CONNECTIONS
#     define PO_SERVER_COUNT_CONNECTIONS
#  endif
#endif




// Check the CPP macro configuration
#if PO_SERVER_NUM_LISTENERS < 1
#  error "PO_SERVER_NUM_LISTENERS < 1"
#endif
#if PO_SERVER_MAX_EPOLL_EVENTS < 1
#  error "PO_SERVER_MAX_EPOLL_EVENTS < 1"
#endif
#if PO_SERVER_MAX_CONNECTIONS < 1
#  error "PO_SERVER_MAX_CONNECTIONS < 1"
#endif
#if PO_SERVER_LISTEN_BACKLOG < 1
#  error "PO_SERVER_LISTEN_BACKLOG < 1"
#endif
#if PO_SERVER_CONNECTION_LONG_TIMEOUT < -1
#  error "PO_SERVER_CONNECTION_LONG_TIMEOUT < -1"
#endif
#if PO_SERVER_CONNECTION_WAIT_TIMEOUT < -1
#  error "PO_SERVER_CONNECTION_WAIT_TIMEOUT < -1"
#endif
#if PO_SERVER_CONNECTION_LONG_TIMEOUT > 0 &&\
    PO_SERVER_CONNECTION_WAIT_TIMEOUT > 0 &&\
    PO_SERVER_CONNECTION_WAIT_TIMEOUT >=\
    PO_SERVER_CONNECTION_LONG_TIMEOUT
#  error "fix PO_SERVER_CONNECTION_LONG_TIMEOUT "
#  error "and/or PO_SERVER_CONNECTION_WAIT_TIMEOUT"
#endif

#ifndef PO_SERVER_NO_THREADPOOL
#  ifndef PO_SERVER_THREADPOOL
#    define PO_SERVER_THREADPOOL // default to threadpool on
#  endif
#else // PO_SERVER_NO_THREADPOOL is defined
#  ifdef PO_SERVER_THREADPOOL
#    error "both PO_SERVER_THREADPOOL and PO_SERVER_NO_THREADPOOL are defined"
#  endif
#endif


#include "aSsert.h"
#include "elog.h"
#include "tIme.h" // for poTime_getDouble()
#ifdef PO_SERVER_THREADPOOL
#  include "pthreadWrap.h"
#  include "threadPool.h"
#else
#  define mutexInit(x)    /*empty macro*/
#  define mutexDestroy(x) /*empty macro*/
#  define mutexLock(x)    /*empty macro*/
#  define mutexUnlock(x)  /*empty macro*/
#  define mutexTryLock(x)  (true) // gets lock every call
#endif


struct POServer_connection;


// Connection callbacks return values:
// returns -1 on error and close
// returns  1 on no error and close
// returns  0 on no error and keep

// RULE:
// The read or write callbacks must consume (or exhaust the I/O) the epoll
// event or remove the callback, if they return 0.  If the read/write
// callback changes in the read/write callback in the callback and does not
// consume the epoll event it must call the new callback so that it may
// consume the epoll event.

typedef int (*POServer_callback_t)(
        struct POServer_connection *conn,
        void *data);



struct POServer_listener
{
    int fd; // file descriptor
    struct sockaddr addr;
    //unsigned short port; // bound to this port
    // next for waiting queue
    struct POServer_listener *next;
    bool waiting; // waiting to call accept because connections are max
    // starting connection readCallback and data
    POServer_callback_t callback;
    void *data;
};

struct POServer_listenerWaitingQueue
{
    // singly link list
    // first is the oldest pulled
    // last is the newest pushed
    struct POServer_listener *first, *last;
};


struct POServer_connection
{
#ifdef PO_SERVER_THREADPOOL
    pthread_mutex_t mutex;
    int changeCount; // incremented when connection use changes
    bool markedForRemoval;
#endif
    int fd;
    double acceptTime; // time of accept() in UNIX time in seconds

    // If this is a connection with a valid fd:
    //   next and prev are used for a time ordered doubly linked list of
    //   connections where
    //   obj->next->acceptTime > obj->acceptTime > obj->prev->acceptTime
    // Else this is an unused connection:
    //   next is used to make list in a queue of unused
    //   struct POServer_connection memory and prev is ignored.
    struct POServer_connection *next, *prev;

    // May be set in callback() with poServer_setNextCallback()
    // the starting values of callback and data comes from the
    // listener.
    
    void *readData, *writeData, *cleanupData;
    POServer_callback_t readCallback, writeCallback, cleanupCallback;
    bool readReady, writeReady; // flag epoll event triggered
    struct POServer *server;
};

// queue of connections unused, singly linked
// just used next to connect them.
struct POServer_connectionQueue
{
    struct POServer_connection *back, *front;
};

// doubly linked list of connections in use
struct POServer_connectionList
{
#ifdef PO_SERVER_COUNT_CONNECTIONS
    uint32_t num; // number of connections open and in use
#endif
    // first is the oldest connection
    // last is the newest connection
    // first->prev == NULL and etc.
    struct POServer_connection *first, *last;
};


struct POServer
{
#ifdef PO_SERVER_THREADPOOL
    struct POThreadPool threadPool;
    pthread_mutex_t mutex;
#endif
    struct POServer_listener listener[PO_SERVER_NUM_LISTENERS];
    uint32_t numListeners;
    struct POServer_listenerWaitingQueue waiting;

    int efd;  // epoll fd
    // We put this event array memory here so it's not regenerated 
    // on the stack for each epoll cycle.
    struct epoll_event event[PO_SERVER_MAX_EPOLL_EVENTS];
 
    /* Full HTTP (and HTTPS) requests are not guaranteed to be read in one
     * epoll cycle, so we will need to save some HTTP state information
     * between epoll_wait() calls.  That's true even if we just supported
     * HTTP/1.0 with no persistent connections.
     * https://en.wikipedia.org/wiki/Stream_Control_Transmission_Protocol
     */

    // conn[] is an array that holds state data associated with
    // connections.  It has an embedded accepting time ordered doubly
    // linked list in it or a unused queue.  A connection struct is in the
    // used list or in the unused queue.
    struct POServer_connection conn[PO_SERVER_MAX_CONNECTIONS];

    struct POServer_connectionList used;    // doubly listed list of open
    struct POServer_connectionQueue unused; // a queue unused structs
    struct POServer *next, *prev; // list of servers for poTato object
    volatile bool isRunning; // a flag to stop user from looping in
    // poServer_doNextEvents()
    bool listenersDown; // flag that marks that all the listening
    // sockets have been closed.
};


extern
struct POServer *poServer_create(struct POServer *s);

extern
bool poServer_listenerSetup(struct POServer *s,
        const char *port, const char *address,
        POServer_callback_t callback, void *data);

extern
void poServer_destroy(struct POServer *s);

extern
void poServer_shutdown(struct POServer *s);

extern
void poServer_doNextEvents(struct POServer *s);


extern
int poServer_echoRequest(struct POServer_connection *conn, void *data);


// to be used in connection callback
extern
int poServer_setReadCallback(struct POServer_connection *conn,
        POServer_callback_t callback, void *data);
// to be used in connection callback
extern
int poServer_setWriteCallback(struct POServer_connection *conn,
        POServer_callback_t callback, void *data);
// to be used in connection callback
extern
void poServer_setCleanupCallback(struct POServer_connection *conn,
        POServer_callback_t callback, void *data);

static inline
bool poServer_isRunning(struct POServer *s)
{
   DASSERT(s);
   return s->isRunning;
}

// semi-private
extern
bool _poServer_addListenerToEpoll(const struct POServer *s,
        const struct POServer_listener *l, int fd);

#endif // #ifndef __PO_SERVER_H__
