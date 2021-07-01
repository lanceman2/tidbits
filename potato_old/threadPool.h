#ifndef __PO_THREADPOOL_H__
#define __PO_THREADPOOL_H__


#include <stdbool.h>
#include <inttypes.h>
#include <pthread.h>

#include "configure.h"




#ifndef PO_THREADPOOL_QUEUE_LENGTH
// 0 for no queue
#  define PO_THREADPOOL_QUEUE_LENGTH     10
#endif

#ifndef PO_THREADPOOL_MAX_NUM_THREADS
#  define PO_THREADPOOL_MAX_NUM_THREADS   5
#endif

#ifndef PO_THREADPOOL_MAX_IDLE_TIME
// 1000 milli seconds = 1 second
// -1 for infinite timeout
//  0 for thread return if there is no task, i.e. no idle threads
#  define PO_THREADPOOL_MAX_IDLE_TIME   1000 // in milli seconds
#endif


// Check for bad values
#if PO_THREADPOOL_QUEUE_LENGTH < 0
#  error "PO_THREADPOOL_QUEUE_LENGTH < 0"
#endif
#if PO_THREADPOOL_MAX_NUM_THREADS < 0
#  error "PO_THREADPOOL_MAX_NUM_THREADS < 0"
#endif
#if PO_THREADPOOL_MAX_IDLE_TIME < -1
#  error "PO_THREADPOOL_MAX_IDLE_TIME < -1"
#endif


#  include "aSsert.h"
#  include "elog.h"

#if PO_THREADPOOL_MAX_IDLE_TIME > 0
#  include "tIme.h" // for poTime_getDouble()
#endif




#if PO_THREADPOOL_MAX_NUM_THREADS > 0


struct POThreadPool_task
{
    void *(*userCallback)(void *);
    void *userData;
};

#if PO_THREADPOOL_QUEUE_LENGTH > 0
struct POThreadPool_taskQueue
{
    struct POThreadPool_task task[PO_THREADPOOL_QUEUE_LENGTH];
    uint32_t nextReadIndex,
             num; // number of entries in the queue
};
#endif

struct POThreadPool_worker
{
    // We must have a pool mutex lock to access any of this struct data

    pthread_t thread;

    struct POThreadPool *pool;

    void *(*userCallback)(void *);
    void *userData;

    // next is for idle list or unused list
    struct POThreadPool_worker *next, *prev;

    double lastWorkTime;

    pthread_cond_t cond;
};

struct POThreadPool_workerList
{
#ifdef PO_DEBUG
    uint32_t length;
#endif
    struct POThreadPool_worker *top, *bottom;
};

struct POThreadPool
{
    uint32_t num_threads; // number of threads that now exist

#if PO_THREADPOOL_QUEUE_LENGTH > 0
    // task queue
    struct POThreadPool_taskQueue task;
#else
    // no task queue but we need to be able to hold one task
    struct POThreadPool_task task;
#endif

    struct POThreadPool_worker worker[PO_THREADPOOL_MAX_NUM_THREADS];

    // Their are three kinds of workers in worker[] array:
    // idle, unused, and working.

#if PO_THREADPOOL_MAX_IDLE_TIME != 0
    // List of idle worker threads that are blocking.
    struct POThreadPool_workerList idle;
#endif

    // List of unused worker structs that have no thread in existence.
    struct POThreadPool_workerList unused;
    // and working threads are not in a list.

    // The worker structs in worker[] are listed in idle or unused, or
    // threads that are not in a list.


    pthread_mutex_t mutex;
    pthread_cond_t cond;

    // flag for when main (master) thread is blocking
    // i.e. when calling pthread_cond_wait()
    bool cleanup; // blocking in poThreadPool_destroy()
    bool detachThread; // tells threads to detach before returning.

    // flag masks that poThreadPool_runTask() is calling
    // pthread_cond_wait().
    bool taskWaitingToBeRun;
};


extern
struct POThreadPool *poThreadPool_create(struct POThreadPool *p);

// Waits for current jobs/requests to finish and then cleans up.  It may
// take 10 seconds, or more, or less; it depends what the code is doing.
extern
bool poThreadPool_destroy(struct POThreadPool *p);

// pull the plug.  May screw things up, like half written files from a
// worker thread, etc.  It still may be a little saver than calling exit()
// or signaling the process.
extern
bool poThreadPool_destroyForce(struct POThreadPool *p);


// None of these functions interrupt the working threads when they are
// calling the user callback, except poThreadPool_destroyForce().


extern
bool poThreadPool_runTask(struct POThreadPool *p,
            void *(*callback)(void *), void *callbackData);

// this is also done in poThreadPool_runTask()
extern
bool poThreadPool_checkIdleThreadTimeout(struct POThreadPool *p);


#else // NOT #if PO_THREADPOOL_MAX_NUM_THREADS > 0

struct POThreadPool
{
};

static inline
struct POThreadPool *poThreadPool_create(struct POThreadPool *p)
{
    return p;
}

static inline
bool poThreadPool_destroyForce(struct POThreadPool *p)
{
    return false;
}

static inline
bool poThreadPool_runTask(struct POThreadPool *p,
            void *(*callback)(void *), void *callbackData)
{
    callback(callbackData);
    return false;
}

static inline
bool poThreadPool_checkIdleThreadTimeout(struct POThreadPool *p)
{
    return false;
}

// We don't inline this just so there is one function defined
// in threadPool.c.
extern
bool poThreadPool_destroy(struct POThreadPool *p);

#endif // #if PO_THREADPOOL_MAX_NUM_THREADS > 0

#endif // #ifndef __PO_THREADPOOL_H__
