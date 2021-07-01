//#define PO_DEBUG
#define PO_ASSERT_TERMINAL
//#define PO_ELOG_DEBUG
#define PO_ELOG_ERROR

#define PO_THREADPOOL_QUEUE_LENGTH      30 /*0 min*/
#define PO_THREADPOOL_MAX_NUM_THREADS   100 /*0 min*/

// in milli seconds,  1 second = 1000 milli seconds
// removes idle threads in this timeout
#define PO_THREADPOOL_MAX_IDLE_TIME     1000 /*-1 -> infinity, 0 -> no time*/
