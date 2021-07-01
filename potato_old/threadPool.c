#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include "threadPool.h"


// Welcome to multi-threaded CPP macro hell!
//
// This shit is flexible and optimal by removing run time switching and
// configuration via CPP macro hell.

// TODO: fix error checking to have goto cleanup and returns

// Note: Reading and writing errno is thread safe.  errno is a magic CPP
// macro that looks like a global variable.




#if PO_THREADPOOL_MAX_NUM_THREADS > 0

#include "pthreadWrap.h" // mutexInit() mutexLock() etc...


// lock pool mutex to call
static inline
void listPush(struct POThreadPool_workerList  *list,
        struct POThreadPool_worker *w)
{
    DASSERT(list);
    DASSERT(w);
    DASSERT(list->length < PO_THREADPOOL_MAX_NUM_THREADS);
    DASSERT(!list->top || list->top->prev == NULL);
    DASSERT(!list->bottom || list->bottom->next == NULL);
    DASSERT((!list->top && !list->bottom) || (list->top && list->bottom));

#ifdef PO_DEBUG
    ++list->length;
#endif

    if(!list->top)
    {
        list->top = list->bottom = w;
        w->prev = w->next = NULL;
        return;
    }

    list->top->prev = w;
    w->next = list->top;
    w->prev = NULL;
    list->top = w;
}

// remove and return an entry from the bottom
// returns NULL if list is empty
static inline
struct POThreadPool_worker
*listPull(struct POThreadPool_workerList *list)
{
    DASSERT(list);
    DASSERT(list->length <= PO_THREADPOOL_MAX_NUM_THREADS);
    DASSERT(!list->top || list->top->prev == NULL);
    DASSERT(!list->bottom || list->bottom->next == NULL);
    DASSERT((!list->top && !list->bottom) || (list->top && list->bottom));

    if(!list->top)
    {
        DASSERT(list->length == 0);
        return NULL;
    }

#ifdef PO_DEBUG
    DASSERT(list->length != 0);
    --list->length;
#endif

    struct POThreadPool_worker *w;
    w = list->bottom;

    if(w->prev)
        w->prev->next = NULL;
    else
        list->top = NULL;

    list->bottom = w->prev;
    return w;
}

// lock pool mutex to call
// returns NULL if list is empty
static inline
struct POThreadPool_worker
*listPop(struct POThreadPool_workerList *list)
{
    DASSERT(list);
    DASSERT(list->length <= PO_THREADPOOL_MAX_NUM_THREADS);
    DASSERT(!list->top || list->top->prev == NULL);
    DASSERT(!list->bottom || list->bottom->next == NULL);
    DASSERT((!list->top && !list->bottom) || (list->top && list->bottom));

    if(!list->top)
    {
        DASSERT(list->length == 0);
        return NULL;
    }

#ifdef PO_DEBUG
    DASSERT(list->length != 0);
    --list->length;
#endif
    struct POThreadPool_worker *w;
    w = list->top;

    if(w->next)
        w->next->prev = NULL;
    else
        list->bottom = NULL;

    list->top = w->next;
    return w;
}

#if PO_THREADPOOL_QUEUE_LENGTH > 0
static inline
bool queueCheck(struct POThreadPool_taskQueue *q)
{
    DASSERT(q);
    DASSERT(q->nextReadIndex < PO_THREADPOOL_QUEUE_LENGTH);
    DASSERT(q->num <= PO_THREADPOOL_QUEUE_LENGTH);
 
    return q->num > 0;
}

// lock pool mutex to call
// Returns the oldest task that was in the queue
// Returns NULL is the queue was empty
static inline
struct POThreadPool_task *queueGetNext(struct POThreadPool_taskQueue *q)
{
    DASSERT(q);
    DASSERT(q->nextReadIndex < PO_THREADPOOL_QUEUE_LENGTH);
    DASSERT(q->num <= PO_THREADPOOL_QUEUE_LENGTH);
    
    if(q->num == 0)
    {
        return NULL;
    }

    uint32_t i;

    i = q->nextReadIndex;
    if(i < PO_THREADPOOL_QUEUE_LENGTH - 1)
        ++q->nextReadIndex;
    else
        q->nextReadIndex = 0;

    --q->num;

    return &q->task[i];
}

// lock pool mutex to call
// Return true if a task that is new made in the queue
// or false if the queue is full and one can't be made.
static inline
bool queueMakeNewEntry(struct POThreadPool_taskQueue *q,
        void *(*userCallback)(void *), void *userData)
{
    DASSERT(q);
    DASSERT(q->nextReadIndex < PO_THREADPOOL_QUEUE_LENGTH);
    DASSERT(q->num <= PO_THREADPOOL_QUEUE_LENGTH);

    if(q->num == PO_THREADPOOL_QUEUE_LENGTH)
        return false; // no new entry, queue if full

    struct POThreadPool_task *task;

    task = &q->task[(q->nextReadIndex + (q->num++))%
        PO_THREADPOOL_QUEUE_LENGTH];
    task->userCallback = userCallback;
    task->userData = userData;
    return true; // We made a new entry
}
#endif // #if PO_THREADPOOL_QUEUE_LENGTH > 0


struct POThreadPool *poThreadPool_create(struct POThreadPool *p)
{
    DASSERT(p);
    memset(p, 0, sizeof(*p));

    struct POThreadPool_worker *worker;
    worker = p->worker;
#ifdef PO_DEBUG
    p->unused.length = PO_THREADPOOL_MAX_NUM_THREADS;
#endif

    int i;
    for(i=0;i<PO_THREADPOOL_MAX_NUM_THREADS;++i)
    {
        worker[i].pool = p;

        if(condInit(&worker[i].cond))
            return NULL; // error

        // Setup the list of unused workers.
        if(i != 0)
        {
            worker[i-1].next = &worker[i];
            worker[i].prev = &worker[i-1];
        }
        else
        {
            worker[PO_THREADPOOL_MAX_NUM_THREADS-1].next = NULL;
            p->unused.top = p->worker;
            p->unused.bottom = &p->worker[PO_THREADPOOL_MAX_NUM_THREADS-1];
            worker[i].prev = NULL;
        }
    }

    // threads detach before returning/exiting except in some cases.
    p->detachThread = true;

    if(mutexInit(&p->mutex) || condInit(&p->cond))
        return NULL; // fail

    return p; // success
}

// cleans up mutexes, conditionals and zeros struct memory
static inline
bool _poThreadPool_destroy(struct POThreadPool *p)
{
    DASSERT(p);

    bool ret = false; // default success

    int i;
    for(i=0;i<PO_THREADPOOL_MAX_NUM_THREADS;++i)
        if(condDestroy(&p->worker[i].cond))
            ret = true; // error

    if(mutexDestroy(&p->mutex) || condDestroy(&p->cond))
        ret = true; // error

#ifdef PO_DEBUG
    // good for debugging
    memset(p, 0, sizeof(*p));
#endif
    return ret;
}


bool poThreadPool_destroy(struct POThreadPool *p)
{
    ILOG("finishing up\n");

#if 0 // testing
#include <unistd.h>
    // to test cleaning up idle threads
    usleep(100000);
#endif

    mutexLock(&p->mutex);

#if PO_THREADPOOL_MAX_IDLE_TIME != 0 && PO_THREADPOOL_QUEUE_LENGTH > 0
    // We should not have idle worker threads and task requests
    DVASSERT(!p->idle.top || !p->task.num,
            "we have %"PRIu32" idle threads and %"PRIu32" task requests\n",
            p->idle.length, p->task.num);
#endif

#if PO_THREADPOOL_MAX_IDLE_TIME != 0

    struct POThreadPool_worker *worker;

    if((worker = listPop(&p->idle)))
    {
        ILOG("waiting for %"PRIu32" idle threads to return\n",
                p->num_threads);

        p->detachThread = false; // we will join the idle threads

        while(worker)
        {
            worker->userCallback = NULL;
            ASSERT((errno = pthread_cond_signal(&worker->cond)) == 0);

            mutexUnlock(&p->mutex);

            ASSERT((errno = pthread_join(worker->thread, NULL)) == 0);

            mutexLock(&p->mutex);
            
            // get next idle thread
            worker = listPop(&p->idle);
        }
    }

    p->detachThread = true; // we will not join the other threads
#endif

    if(p->num_threads > 0)
    {
        // no idle workers but we have tasks requests
        // and working threads

        // we must have working threads that we wait for here.
#if PO_THREADPOOL_QUEUE_LENGTH > 0
        ILOG("waiting for %"PRIu32" working threads "
                "and %"PRIu32" queued tasks\n",
                p->num_threads, p->task.num);
#else
        ILOG("waiting for %"PRIu32" working threads\n",
                p->num_threads);
#endif

        p->cleanup = true;
        condWait(&p->cond, &p->mutex);
    }

    mutexUnlock(&p->mutex);
#ifdef PO_ELOG_INFO
    bool ret;
    ret = _poThreadPool_destroy(p);
    ILOG("finished threadPool destruction\n");
    return ret;
#else
    return _poThreadPool_destroy(p);
#endif
}

bool poThreadPool_destroyForce(struct POThreadPool *p)
{
    bool ret = false; // default no error
 
    // TODO: stuff here

    if(_poThreadPool_destroy(p))
        ret = true;

    return ret;
}

typedef void *(*_poThreadPool_callback_t)(void *);

static inline
_poThreadPool_callback_t lookForWork(struct POThreadPool *p,
        struct POThreadPool_worker *worker,
        void **userData)
{
#if PO_THREADPOOL_QUEUE_LENGTH > 0
    struct POThreadPool_task *task;
    if((task = queueGetNext(&p->task)))
    {
        // We found a new task.

        DVASSERT(p->unused.top == NULL,
                "there were unused workers and a task in the queue\n");

#  if PO_THREADPOOL_MAX_IDLE_TIME != 0
        DVASSERT(p->idle.top == NULL,
                "there were idle workers and a task in the queue\n");
#  endif

        if(p->taskWaitingToBeRun)
            // The task queue was full just before we called
            // queueGetNext() above. Any time the task queue is filled
            // the master thread will block and wait for this signal.
            ASSERT((errno = pthread_cond_signal(&p->cond)) == 0);

        *userData = task->userData;
        return task->userCallback;
    }
#else // NOT #if PO_THREADPOOL_QUEUE_LENGTH > 0

    if(p->taskWaitingToBeRun)
    {
        ASSERT((errno = pthread_cond_signal(&p->cond)) == 0);
        *userData = p->task.userData;
        return p->task.userCallback;
    }
#endif // #if PO_THREADPOOL_QUEUE_LENGTH > 0
    return NULL;
 }

static
void *workerCallback(struct POThreadPool_worker *worker)
{
    DASSERT(worker);
    DASSERT(worker->pool);
    struct POThreadPool *p;
    pthread_mutex_t *pmutex /*pool mutex*/;
    p = worker->pool;
    pmutex = &p->mutex;


         /////////////////////////////////////////////////|
        ////////////// ACCESSING POOL DATA ////////////////|
       /////////////////////////////////////////////////////|
      /////                                              \///|
     /*-*/              mutexLock(pmutex);                ////|
    /////                                                  \///|
        
    void *ret;
    void *(*userCallback)(void *);
    void *userData;

    // copy data to the list in case things change while we did not
    // have the pmutex lock while we were calling the user callback.
    userCallback = worker->userCallback;
    userData = worker->userData;

    ++p->num_threads;
    DASSERT(p->num_threads <= PO_THREADPOOL_MAX_NUM_THREADS);

    // extra spaces to line up with other print below
    ILOG("  adding thread, there are now %"PRIu32" threads\n",
                p->num_threads);

    // now tell the manager thread to proceed
    ASSERT((errno = pthread_cond_signal(&p->cond)) == 0);

   
    /********************************************************************
     * Because of the pool mutex is used by all threads, from the pool
     * manager's (main thread) point of view this thread is either in the
     * idle queue and waiting, or it is running the callback.  These are
     * the only two places where this thread does not hold the pool mutex
     * lock.
     ********************************************************************/

    while(true)
    {

        ////|                                                  /////
         /*-*/             mutexUnlock(pmutex);               /////
          /////                                              /////
           //////////////////////////////////////////////////////
            /////////// FINISHED ACCESSING POOL DATA ///////////
             //////////////////////////////////////////////////


        //////////////// go to work on the task ///////////////////
        ret = userCallback(userData); // working callback
        ////////////////// finished work on task //////////////////


        DLOG("thread id %lu finished task\n",
                (unsigned long) pthread_self());


             /////////////////////////////////////////////////|
            ////////////// ACCESSING POOL DATA ////////////////|
           /////////////////////////////////////////////////////|
          /////                                              \///|
         /*-*/              mutexLock(pmutex);                ////|
        /////                                                  \///|

        if((userCallback = lookForWork(p, worker, &userData)))
            continue;

        if(p->cleanup)
        {
            if(p->num_threads == 1)
                // This is the last thread.  Signal the master
                // on the way out.
                ASSERT((errno = pthread_cond_signal(&p->cond)) == 0);
            break;
        }

#if PO_THREADPOOL_MAX_IDLE_TIME != 0
        // So we can tell if we get a new task after this.
        worker->userCallback = NULL;

#  if PO_THREADPOOL_MAX_IDLE_TIME > 0
        worker->lastWorkTime = poTime_getDouble();
#  endif

        // Put this worker in the idle worker list so that
        // the managing thread can signal it.
        listPush(&p->idle, worker);

        ///////////////////////////////////////////////////////////////
        ////////////////////// IDLE WORKER SLEEP //////////////////////
        ///////////////////////////////////////////////////////////////
        // We wait until the manager pops this worker off for new task
        // or finish running.
        //
        // pthread_cond_wait() does unlock pmutex, wait for signal,
        // and then lock pmutex
        condWait(&worker->cond, pmutex);
        // Note: a thread can sit here an arbitrary amount of time,
        // even if it is signaled.
        ///////////////////////////////////////////////////////////////


        if(worker->userCallback)
        {
            // We have a new task.
            userCallback = worker->userCallback;
            userData = worker->userData;
            continue;
        }
        if((userCallback = lookForWork(p, worker, &userData)))
            continue;

#endif // #if PO_THREADPOOL_MAX_IDLE_TIME != 0

        break; // will return

    } ///////////////////////////// while(true) loop


    listPush(&p->unused, worker);

    --p->num_threads;
    DASSERT(p->num_threads < PO_THREADPOOL_MAX_NUM_THREADS);

    ILOG("removing thread, there are now %"PRIu32" threads\n",
                p->num_threads);

#if PO_THREADPOOL_QUEUE_LENGTH > 0
    // This thread is returning so the task queue should be empty.
    DASSERT(p->task.num == 0);
#endif

    if(p->detachThread)
        ASSERT((errno = pthread_detach(pthread_self())) == 0);


    ////|                                                  /////
     /*-*/             mutexUnlock(pmutex);               /////
      /////                                              /////
       //////////////////////////////////////////////////////
        /////////// FINISHED ACCESSING POOL DATA ///////////
         //////////////////////////////////////////////////



    return ret;
}

#if PO_THREADPOOL_MAX_IDLE_TIME > 0
// get pool mutex lock before calling this
static
bool _poThreadPool_checkThreadTimeout(struct POThreadPool *p)
{
    if(p->num_threads < 2) return false;

    double t;
    t = poTime_getDouble();

    // We just remove one worker thread.  Removing many threads
    // at once can cause problems in many apps.  We assuming that
    // this function is called regularly.

    if(p->num_threads > 2 && p->idle.bottom &&
            t - p->idle.bottom->lastWorkTime >
            (PO_THREADPOOL_MAX_IDLE_TIME/1000.0))
    {
        struct POThreadPool_worker *worker;
        worker = listPull(&p->idle);
        DASSERT(worker);
        DASSERT(worker->userCallback == NULL);
        ASSERT((errno = pthread_cond_signal(&worker->cond)) == 0);
        DLOG("removing thread %ld idle %.2lf seconds\n",
                (unsigned long) worker->thread,
                t - worker->lastWorkTime);
    }
    return false; // success
}
#endif

bool poThreadPool_checkIdleThreadTimeout(struct POThreadPool *p)
{
#if PO_THREADPOOL_MAX_IDLE_TIME > 0
    DASSERT(p);

    mutexLock(&p->mutex);

#if PO_THREADPOOL_QUEUE_LENGTH > 0
    // We should not have idle worker threads and task requests
    DVASSERT(!p->idle.top || !p->task.num,
            "we have %"PRIu32" idle threads and %"PRIu32" task requests\n",
            p->idle.length, p->task.num);
#endif

    DASSERT(p->num_threads <= PO_THREADPOOL_MAX_NUM_THREADS);

    bool ret;
    ret = _poThreadPool_checkThreadTimeout(p);
    
    mutexUnlock(&p->mutex);
    return ret;
#else // #if PO_THREADPOOL_MAX_IDLE_TIME > 0
    return false; // success
#endif // #if PO_THREADPOOL_MAX_IDLE_TIME > 0
}


// This will block until we can fit the job into our work schedule.  We
// can add automatic and explicit timeouts and shit so that bad jobs do
// not block this forever.
bool poThreadPool_runTask(struct POThreadPool *p,
            void *(*userCallback)(void *), void *userData)
{
    DASSERT(p);
    DASSERT(userCallback);

    pthread_mutex_t *pmutex;
    pmutex = &p->mutex;

    mutexLock(pmutex);

#if PO_THREADPOOL_MAX_IDLE_TIME != 0 && PO_THREADPOOL_QUEUE_LENGTH > 0
    // We should not have idle worker threads and task requests
    DVASSERT(!p->idle.top || !p->task.num,
            "we have %"PRIu32" idle threads and %"PRIu32" task requests\n",
            p->idle.length, p->task.num);
#endif

    DASSERT(p->num_threads <= PO_THREADPOOL_MAX_NUM_THREADS);

    struct POThreadPool_worker *worker;

    while(true)
    {

#if PO_THREADPOOL_MAX_IDLE_TIME != 0

        if((worker = listPop(&p->idle)))
        {
            // We have an idle worker thread ready to do this task
            ASSERT((errno = pthread_cond_signal(&worker->cond)) == 0);
            worker->userCallback = userCallback;
            worker->userData = userData;

#if PO_THREADPOOL_MAX_IDLE_TIME > 0
            _poThreadPool_checkThreadTimeout(p);
#endif
            // Now the thread should be on it's way to working
            // on this task as soon as we unlock the pool mutex.
            break;
        }
        else
#endif // #if PO_THREADPOOL_MAX_IDLE_TIME != 0

            if((worker = listPop(&p->unused)))
        {
            // We can make a new worker thread for this task
            pthread_attr_t attr;
            pthread_t thread;
            DASSERT(p->num_threads < PO_THREADPOOL_MAX_NUM_THREADS);
            ASSERT((errno = pthread_attr_init(&attr)) == 0);
#if 0
            size_t stackSize;
            ASSERT((errno = pthread_attr_getstacksize(&attr, &stackSize)) == 0);
            ELOG("stacksize= %zu MBytes\n", stackSize/(1024*1024));
#endif
#ifdef PO_THREADPOOL_STACKSIZE
            ASSERT((errno = pthread_attr_setstacksize(&attr,
                            PO_THREADPOOL_STACKSIZE)) == 0);
#endif
            worker->userCallback = userCallback;
            worker->userData = userData;
            ASSERT((errno = pthread_create(&thread, &attr,
                    (void *(*)(void *)) workerCallback,
                    worker)) == 0);

            worker->thread = thread;
            // wait for thread to start so that we can't change
            // anything while the thread is getting info to start.
            condWait(&p->cond, pmutex);
            break;
        }
        else
#if PO_THREADPOOL_QUEUE_LENGTH > 0
            
            if(queueMakeNewEntry(&p->task, userCallback, userData))
        {
            // The idle list is empty and the unused worker list is empty
            // and the task queue is not full
            DLOG("there are %"PRIu32" task requests queued\n",
                    p->task.num);
            break;
        }
        else
#endif
        {
            // the idle list is empty and the unused work list is empty
            // and task queue is full and we could not add an entry
#if PO_THREADPOOL_QUEUE_LENGTH < 1
            DLOG("max %"PRIu32" working threads NOW BLOCKING\n",
                    p->num_threads);
#else
            DLOG("max %"PRIu32" working threads and max %"PRIu32" "
                    "tasks queued NOW BLOCKING\n",
                    p->num_threads, p->task.num);
#endif

            p->taskWaitingToBeRun = true;


#if PO_THREADPOOL_QUEUE_LENGTH < 1
            // There's no task queue and no room for more threads, so we
            // just give the task to the next thread to see the
            // p->taskWaitingToBeRun flag in it's thread loop.
            p->task.userCallback = userCallback;
            p->task.userData = userData;
            // wait until a thread grabs it.
            // release lock, wait and wake, have lock when it returns
            condWait(&p->cond, pmutex);
            p->taskWaitingToBeRun = false;
            break;
#else
            // In this case we must loop more so that we use the task
            // queue to get the task that is first in line and not this
            // new task.
            //
            // wait until a thread signals.
            //
            // release lock, wait and wake, have lock when it returns
            condWait(&p->cond, pmutex);
            p->taskWaitingToBeRun = false;
#endif
        }
    }

    DLOG("there are now %"PRIu32" threads\n", p->num_threads);

    mutexUnlock(pmutex);

    return false; // success
}

#else // #if PO_THREADPOOL_MAX_NUM_THREADS > 0


bool poThreadPool_destroy(struct POThreadPool *p)
{
    return false;
}

#endif // #if PO_THREADPOOL_MAX_NUM_THREADS > 0
