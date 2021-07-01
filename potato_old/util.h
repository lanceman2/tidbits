#ifndef __PO_UTIL_H__
#define __PO_UTIL_H__

#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>

#include "aSsert.h"


static inline
bool readn(int fd, void *ptr, size_t n)
{
    errno = 0;
    uint8_t *buf;
    buf = ptr;
    while(n)
    {
        ssize_t rt;
        rt = read(fd, buf, n);
        if(rt > 0)
        {
            buf += rt;
            n -= rt;
        }
        else if(rt == 0)
        {
            WLOG("read()=%zd\n", rt);
            return true;
        }
        else
        {
            WLOG("read()=%zd failed\n", rt);
            DVASSERT(0, "read()=%zd failed\n", rt);
            return true;
        }
    }
    return false; // success
}

static inline
bool writen(int fd, const void *ptr, size_t n)
{
    errno = 0;
    const uint8_t *buf;
    buf = ptr;
    while(n)
    {
        ssize_t rt;
        rt = write(fd, buf, n);
        if(rt > 0)
        {
            buf += rt;
            n -= rt;
        }
        else
        {
            WLOG("write()=%zd failed\n", rt);
            DVASSERT(0, "write()=%zd failed\n", rt);
            return true;
        }
    }
    return false; // success
}

static inline bool
setNonblocking(int fd)
{
    int flags;
    return ASSERT((flags = fcntl(fd, F_GETFL, 0)) != -1) ||
        ASSERT(flags & O_NONBLOCK ||
            fcntl(fd, F_SETFL, flags|O_NONBLOCK) != -1);
}

static inline bool
poUtil_setAddress(struct sockaddr *saddr, const char *port,
        const char *host, const char *func)
{
    memset(saddr, 0, sizeof(*saddr));

// TODO: remove OLD code below
//#define OLD

#ifdef OLD

    DASSERT(sizeof(uint16_t) == sizeof(unsigned short));
    uint16_t portNum;
    {
        unsigned long ul;
        char *endptr = NULL;
        ul = strtoul(port, &endptr, 10);
    
        if(VASSERT(endptr != port,
            "%s() failed: strtoul(port=\"%s\",,) failed\n", func, port))
            return true; // error
        if(VASSERT(ul <= 0xFFFF && ul != 0,
            "%s() failed: port=\"%s\"\n", func, port))
            return true;
        portNum = ul;
    }

    bool ip6 = false;
    const char *s;
    for(s=host; *s; ++s)
    {
        if(*s == ':')
        {
            ip6 = true;
            break;
        }
        if(*s == '.')
            break;
    }

    if(!*s)
    {
#endif
    // host is like for example "google.com" or "localhost"
    // not "127.0.0.1" or "2001:db8:85a3:0:0:8a2e:370:7334" (ipv6)

    struct addrinfo hints, *info = NULL, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;  /* Any protocol */
    //hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */

    errno = 0;
    int rt;

    rt = getaddrinfo(host, port, &hints , &info);

    if(VASSERT(rt == 0,
                "getaddrinfo(\"%s\", \"%s\",,) failed- "
                "err=%d \"%s\"\n", host, port,
                rt, gai_strerror(rt)))
        return true; // error

    DASSERT(info);

    for(p=info; p; p=p->ai_next)
    {
        // We are looking for particular types of addresses.  Not shit
        // that we don't know how to use.
        if((p->ai_family == AF_INET ||
                p->ai_family == AF_INET6) &&
            p->ai_addrlen <= sizeof(*saddr))
            break;
    }

    if(VASSERT(p,
        "getaddrinfo(\"%s\", \"%s\",,) failed"
        " to find good address\n",
        host, port))
    {
        freeaddrinfo(info);
        return true; // error
    }

    memcpy(saddr, p->ai_addr, p->ai_addrlen);
    freeaddrinfo(info);
    return false; // success

#ifdef OLD
    }

    if(ip6) // IP version 6
    {
        struct sockaddr_in6 *addr;
        addr = (struct sockaddr_in6 *) saddr;
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(portNum);
        ASSERT(inet_pton(AF_INET6, host, &addr->sin6_addr) == 1);
        return false;
    }
    // else // ipv4
    {
        struct sockaddr_in *addr;
        addr = (struct sockaddr_in *) saddr;
        addr->sin_family = AF_INET;
        addr->sin_port = htons(portNum);
        ASSERT(inet_pton(AF_INET, host, &addr->sin_addr) == 1);
        //addr->sin_addr.s_addr = htonl(INADDR_ANY);
        return false; // success
    }

#endif

}

#endif //#ifndef __PO_UTIL_H__
