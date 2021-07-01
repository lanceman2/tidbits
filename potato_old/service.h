#ifndef __PO_SERVICE_H__
#define __PO_SERVICE_H__

#include <stdbool.h>
#include <errno.h>

#include "configure.h"
#include "server.h"
#include "tls.h"

#if EWOULDBLOCK != EAGAIN
#  error "coding assumption error: errno values: EWOULDBLOCK != EAGAIN"
#endif

#ifndef PO_SERVICE_CHUNKLEN
#  define PO_SERVICE_CHUNKLEN   (1023)
#endif

#if PO_SERVICE_CHUNKLEN < 2
#  error "PO_SERVICE_CHUNKLEN < 2"
#endif

#ifndef PO_SERVICE_NO_FLOCK
#  ifndef PO_SERVICE_FLOCK
#    define PO_SERVICE_FLOCK // default use flock() to read files
#  endif
#else
#  undef PO_SERVICE_NO_FLOCK
#endif


// This class turns TCP from stream-like to message-like and callback
// based.


enum POServiceReturn
{
    // the POService_requestCallback may return the following:

    POSERVICE_CONTINUE, // message was complete, keep the socket
    POSERVICE_WAIT,     // wait for more request data and call again
    POSERVICE_CLOSE,    // close socket when sends are complete
    POSERVICE_ERROR     // close socket with error, stop all sends

    // open and reading, continue to call requestCallback with new
    // requests on the same socket connection.  The memory of the buffer
    // is managed by the Service.
};


struct POService;
struct POService_session;

typedef enum POServiceReturn (*POService_requestCallback_t)
    (struct POService_session *s, void *buffer, size_t bytes, void *data);
typedef void (*POService_cleanupCallback_t)
    (struct POService_session *s, void *data);


struct POService_session;

struct POService
{
    size_t maxMessageLength; // input message length
    struct POTls *tls; // NULL if not used
    POService_requestCallback_t callback; // user read func
    void *callbackData; // user read data
};

struct POService_session
{
    struct POService *service;
    struct POServer_connection *conn;
    uint8_t *inBuffer;
    size_t inSize/*size allocated - 1*/, inLen/*used*/;
    // output queue
    struct Out *firstOut, *lastOut;
    gnutls_session_t tlsSession; // 0 if not used
    // user session read function and callback data
    POService_requestCallback_t sessionCallback;
    void *sessionCallbackData;
    POService_cleanupCallback_t cleanup;
    void *cleanupData;
};

extern
void poService_setRead(struct POService_session *s,
        POService_requestCallback_t callback, void *data);
extern
void poService_setCleanup(struct POService_session *s,
        POService_cleanupCallback_t callback, void *data);

static inline
bool poService_isTLS(const struct POService_session *s)
{
    DASSERT(s);
    return (bool) s->tlsSession;
}

// s is memory passed in
// server is a working server
extern // just TCP
bool poService_create(struct POService *s,
        struct POServer *server,
        struct POTls *tls,
        const char *port, const char *address,
        size_t maxMessageLength,
        POService_requestCallback_t callback,
        void *data);



// free(buffer) will be called for the user.  When the data is sent
// depends on when the socket is ready writing.  If the socket writing
// fails the connection is closed.
extern
bool poService_send(struct POService_session *s,
        void *buffer, size_t bytes);

extern
bool poService_sendF(struct POService_session *s, const char *format, ...)
                        __attribute__((format(printf,2,3)));

// When the data is sent depends on when the socket is ready writing.
// If this fails due to an error not related to the socket the
// errorCallbck will be called.
extern
bool poService_sendPath(struct POService_session *s, const char *path);

extern
bool poService_sendFD(struct POService_session *s, int fd, bool haveFLock);


#endif // #ifndef __PO_SERVICE_H__
