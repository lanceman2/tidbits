
#define PO_DEBUG
#define PO_ASSERT_TERMINAL
#define PO_ELOG_DEBUG

#define PO_SERVER_COUNT_CONNECTIONS
#define PO_SERVER_NO_THREADPOOL

#define PO_SERVER_NUM_LISTENERS               2
#define PO_SERVER_MAX_EPOLL_EVENTS           10
#define PO_SERVER_MAX_CONNECTIONS            10
#define PO_SERVER_LISTEN_BACKLOG             20
#define PO_SERVER_EPOLL_WAIT_TIME           200 // millisec, -1 = infinity
#define PO_SERVER_CONNECTION_LONG_TIMEOUT    -1 // millisec, -1 = infinity
#define PO_SERVER_CONNECTION_WAIT_TIMEOUT   100 // millisec, -1 = infinity

