#define PO_DEBUG
#define PO_ASSERT_TERMINAL
#define PO_ELOG_DEBUG

#define PO_THREADPOOL_QUEUE_LENGTH      5 /*0 min*/
#define PO_THREADPOOL_MAX_NUM_THREADS   13 /*0 min*/

// in milli seconds,  1 second = 1000 milli seconds
#define PO_THREADPOOL_MAX_IDLE_TIME     1000 /*-1 -> infinity, 0 -> no time*/
