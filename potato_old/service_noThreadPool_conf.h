#define PO_DEBUG
#define PO_ASSERT_TERMINAL
#define PO_ELOG_DEBUG
//#define PO_ELOG_WARNING

#define PO_SERVER_COUNT_CONNECTIONS
#define PO_SERVER_NO_THREADPOOL

#define PO_SERVER_NUM_LISTENERS               3
#define PO_SERVER_MAX_EPOLL_EVENTS           20
#define PO_SERVER_MAX_CONNECTIONS            10
#define PO_SERVER_LISTEN_BACKLOG             50
#define PO_SERVER_EPOLL_WAIT_TIME           800 // millisec, -1 = infinity
#define PO_SERVER_CONNECTION_LONG_TIMEOUT  2000 // millisec, -1 = infinity
#define PO_SERVER_CONNECTION_WAIT_TIMEOUT  1000 // millisec, -1 = infinity


#define PO_THREADPOOL_QUEUE_LENGTH      20 /*0 min*/
#define PO_THREADPOOL_MAX_NUM_THREADS   10 /*0 min*/
// in milli seconds,  1 second = 1000 milli seconds
#define PO_THREADPOOL_MAX_IDLE_TIME   2000 /*-1 -> infinity, 0 -> no time*/


// allocating chunk length
#define PO_SERVICE_CHUNKLEN        127 // bytes // small for testing

// If run as uid 0 will switch to uid listed after binding ports.
#define SERVER_TEST_UID                    1000


//#define PO_PTHREADMUTEX_DEBUG

#include "clientAndServiceTest_conf.h"
