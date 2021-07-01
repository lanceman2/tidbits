#ifndef __PO_HTTP_H__
#define __PO_HTTP_H__


#include "configure.h"
#include "service.h"

extern
enum POServiceReturn
poHttp_redirectToHttps(struct POService_session *s,
        void *buffer, size_t bytes, void *data);
extern
enum POServiceReturn
poHttp_read(struct POService_session *s, void *buffer,
        size_t bytes, void *data);

#endif // #ifndef __PO_HTTP_H__
