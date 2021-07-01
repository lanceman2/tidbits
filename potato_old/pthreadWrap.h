#ifndef __PTHREADWRAP_H__
#define __PTHREADWRAP_H__

#include <errno.h>
#include <pthread.h>

#include "aSsert.h"

static inline
bool mutexInit(pthread_mutex_t *mutex)
{
    pthread_mutexattr_t mutexattr;
    return
        ASSERT((errno = pthread_mutexattr_init(&mutexattr)) == 0) ||
        ASSERT((errno = pthread_mutexattr_settype(&mutexattr,
                PTHREAD_MUTEX_FAST_NP)) == 0) ||
        ASSERT((errno = pthread_mutex_init(mutex, &mutexattr)) == 0) ||
        ASSERT((errno = pthread_mutexattr_destroy(&mutexattr)) == 0);
}

static inline
bool _mutexTryLock(pthread_mutex_t *mutex)
{
    errno = pthread_mutex_trylock(mutex);

    switch(errno)
    {
        case 0:
            return true; // we have the lock
        case EBUSY:
            return false; // we do NOT have the lock
        default:
            VASSERT(0, "pthread_mutex_trylock() failed\n");
    }
    return false; // shit
}

static inline
bool _mutexLock(pthread_mutex_t *mutex)
{
    while((errno = pthread_mutex_lock(mutex)) != 0)
    {
        if(errno != EINTR)
            return VASSERT(0, "pthread_mutex_lock() failed");
        DLOG("pthread_mutex_lock() was interrupted by a signal\n");
    }
    return false;
}

#ifdef PO_PTHREADMUTEX_DEBUG

static inline bool
_MutexTryLock(pthread_mutex_t *mutex, const char *file, int line)
{
    bool ret = _mutexTryLock(mutex);
    if(!ret)
        fprintf(PO_ELOG_FILE, "%s:%d %s failed\n",
            file, line, __func__);
    return ret;
}

static inline bool
_MutexLock(pthread_mutex_t *mutex, const char *file, int line)
{
    if(!_mutexTryLock(mutex))
    {
        fprintf(PO_ELOG_FILE, "%s:%d %s blocked\n",
            file, line, __func__);
        return _mutexLock(mutex);
    }
    return false;
}

#  define mutexTryLock(x) _MutexTryLock((x), __BASE_FILE__, __LINE__)
#  define mutexLock(x) _MutexLock((x), __BASE_FILE__, __LINE__)

#else

#  define mutexTryLock(x) _mutexTryLock(x)
#  define mutexLock(x) _mutexLock(x)

#endif




static inline
bool mutexUnlock(pthread_mutex_t *mutex)
{
    return ASSERT((errno = pthread_mutex_unlock(mutex)) == 0);
}

static inline
bool condInit(pthread_cond_t *cond)
{
    return ASSERT((errno = pthread_cond_init(cond, NULL)) == 0);
}

static inline
bool mutexDestroy(pthread_mutex_t *mutex)
{
    return ASSERT((errno = pthread_mutex_destroy(mutex)) == 0);
}

static inline
bool condDestroy(pthread_cond_t *cond)
{
    return ASSERT((errno = pthread_cond_destroy(cond)) == 0);
}

static inline
bool condWait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    errno = 0;
    while((errno = pthread_cond_wait(cond, mutex)) != 0)
    {
        if(errno != EINTR)
            return VASSERT(0, "pthread_cond_wait() failed");
        DLOG("pthread_cond_wait() was interrupted by a signal\n");
    }
    return false;
}

#endif // #ifndef __PTHREADWRAP_H__
