#include <stdbool.h>

#include "http.h"

enum POServiceReturn
poHttp_redirectToHttps(struct POService_session *s,
        void *buffer, size_t bytes, void *data)
{
    DASSERT(s);
    DASSERT(bytes > 0);
    DASSERT(!poService_isTLS(s));

    poService_sendF(s,
            "HTTP/1.1 301 Moved Permanently\r\n"
            "Location: https://"HTTP_HOST":"HTTPS_PORT"\r\n"
            "\r\n");
    return POSERVICE_CLOSE;
}

enum POServiceReturn
poHttp_read(struct POService_session *s,
        void *buffer, size_t bytes, void *data)
{
    DASSERT(bytes > 0);
    char *buf;
    buf = buffer;
    buf[bytes] = '\0';

#ifdef SERVER_SPEW
    static int count = 0; // TODO: count may be garbage in threads.
    printf("%d READ(%zu bytes): %s\n", ++count, bytes, buf);
#endif

    poService_sendF(s, "%s", buf);

    return POSERVICE_CONTINUE;
}
