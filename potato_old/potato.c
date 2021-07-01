#include <errno.h>
#include <arpa/inet.h>
#include <stropts.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sched.h>
#include <sys/wait.h>

#include "server.h"
#include "potato.h"
#include "util.h"

// Just the main thread accesses this stuff
// so we hope.  Thread safety!

#define END_CHAR 'e'

static
struct POTato
{
    bool isInit;
    bool terminating;
    int doOptionsFlag;
    int numForks;
    uint32_t numListeners;
    struct POServer *servers; // list of servers
    struct POTato_handoffSockets *handoffSockets;
    int shutdownFd; // UNIX domain socket used to hand-off
    int exitStatus; // adds up children exit statuses up to 255
    // The return status maxes out at 255.
    pid_t *childPids;
    int numChildren;
    long masterTid; // master thread id, like pid from gettid()
    // thread ID is not the same as pthread id
}
po =
{
    .isInit = false,
    .terminating = false,
    .doOptionsFlag = 0,
    .numForks = 0,
    .numListeners = 0,
    .servers = NULL,
    .handoffSockets = NULL,
    .shutdownFd = -1,
    .exitStatus = 0,
    .childPids = NULL,
    .numChildren = 0,
    .masterTid = 0
};

void poTato_printOptionUsage(FILE *f)
{
    bool gotOne = false;

    if(po.doOptionsFlag | PO_OPTION_COPY_LISTENERS)
    {
        fprintf(f, "[--copy PID]");
        gotOne = true;
    }
    if(po.doOptionsFlag | PO_OPTION_REPLACE_LISTENERS)
    {
        if(gotOne) fprintf(f, "|");
        fprintf(f, "[--replace PID]");
        gotOne = true;
    }
    if(po.doOptionsFlag | PO_OPTION_FORK)
    {
        if(gotOne) fprintf(f, "|");
        fprintf(f, "[--fork NUM]");
    }
}

void poTato_printOptionHelp(FILE *f)
{
    if(po.doOptionsFlag | PO_OPTION_COPY_LISTENERS)
    {
        fprintf(f, "\n"
"  --copy PID     get a copy of listening sockets from server with PID.\n"
"                 This option may be used any number of times\n");
    }
    if(po.doOptionsFlag | PO_OPTION_REPLACE_LISTENERS)
    {
        fprintf(f, "\n"
"  --fork NUM     fork NUM additional potato servers\n");
    }
    if(po.doOptionsFlag | PO_OPTION_FORK)
    {
        fprintf(f, "\n"
"  --replace PID  get a copy of listening sockets from server with PID\n"
"                 and tell server with PID to exit.  This option may be\n"
"                 used any number of times\n");
    }
}

struct POTato_handoffSockets
{
    struct sockaddr addr;
    int fd;
};

static inline
void initSendFdMsg(struct msghdr *msg, struct iovec *iov,
        int fd, char *buf, size_t len)
{
    msg->msg_iov = iov;
    msg->msg_iovlen = 1;
    msg->msg_control = buf;
    msg->msg_controllen = len;
    msg->msg_name = NULL;
    msg->msg_namelen = 0;

    iov->iov_base = buf;
    iov->iov_len = 1;

    struct cmsghdr *header = CMSG_FIRSTHDR(msg);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(sizeof(int));
    //ELOG("header->cmsg_len = %zu\n", header->cmsg_len);
    *(int *)CMSG_DATA(header) = fd;
}

static inline
bool sendFd(int fd, int sfd)
{
    struct msghdr msg;
    struct iovec iov[1];
    char buf[CMSG_SPACE(sizeof(int))];
    initSendFdMsg(&msg, iov, sfd, buf, sizeof(buf));

    if(ASSERT(1 == sendmsg(fd, &msg, 0)))
        return true; // error

    return false; // success
}

static inline
int recvFd(int fd)
{
    struct msghdr msg;
    struct iovec iov;
    char buf[CMSG_SPACE(sizeof(int))];
    initSendFdMsg(&msg, &iov, -1, buf, sizeof(buf));
    
    if(ASSERT(1 == recvmsg(fd, &msg, 0)))
        return -1;

    struct cmsghdr *header;
    header = CMSG_FIRSTHDR(&msg);
    if(ASSERT(header->cmsg_level == SOL_SOCKET &&
                header->cmsg_type == SCM_RIGHTS
                && header->cmsg_len == CMSG_LEN(sizeof(int))))
        return -1;

    return *( (int *)(CMSG_DATA(header)));
}

static
bool setSignal(int sig, void (*handler)(int),
        void (*actionHandler)(int, siginfo_t *, void *))
{
    DASSERT(handler || actionHandler ||
            (!handler && !actionHandler));

    struct sigaction act;
    memset(&act, 0, sizeof(act));

    if(actionHandler == NULL)
        act.sa_handler = handler;
    else
    {
        act.sa_sigaction = actionHandler;
        act.sa_flags = SA_SIGINFO;
    }
    if(ASSERT(sigaction(sig, &act, NULL) == 0))
        return true; // error

    return false;
}



#define HANDOFF_PREFIX "/tmp/poTatoZz_"

static void passListeners(int sig, siginfo_t *siginfo, void *context)
{
    DASSERT(sig == SIGUSR1 || sig == SIGUSR2);
    WLOG("caught signal %d starting listener fd hand-off\n", sig);

    int fd = -1;
    uint32_t numListeners = 0;
    struct POServer *s;
    struct sockaddr_un addr;
    char path[64];

    for(s = po.servers; s; s = s->next)
        numListeners += s->numListeners;

    memset(path, 0, sizeof(path));

    snprintf(path, sizeof(path),
            HANDOFF_PREFIX "%d", getpid());

    if(ASSERT((fd = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0))
        return;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path, sizeof(path));

    if(ASSERT(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0))
        goto fail;

    unlink(path);

    // This protocol is stupid simple:
    if(writen(fd, &numListeners, sizeof(numListeners)))
        goto fail;

    for(s = po.servers; s; s = s->next)
    {
        int32_t i;
        for(i = s->numListeners - 1; i>=0; --i)
        {
            if(writen(fd, &s->listener[i].addr, sizeof(struct sockaddr)))
                goto fail;

            if(sendFd(fd, s->listener[i].fd))
                goto fail;
        }
    }

    WLOG("finished listener fd hand-off\n");


    if(sig == SIGUSR2)
    {
        // We don't shutdown in this case.
        close(fd);
        return;
    }

    // We don't need this signal handler any more.
    if(setSignal(SIGUSR1, SIG_IGN, NULL))
        goto fail;
    if(setSignal(SIGUSR2, SIG_IGN, NULL))
        goto fail;


    // When the last server calls poServer_shutdown() this fd is closed
    // where-by signaling the other server program to continue running via
    // broken read() call.  We do this close later after we close all the
    // listener sockets in all servers.  So we don't get accept() on a
    // closed socket.  We do a proper shutdown, finishing the last loop of
    // poServer_doNextEvent().  We must get all listeners closed before
    // the other server is allowed to continue running, so that the case
    // where a bind address with the same port with a different address on
    // the new server can remove the old binding and make a new binding
    // with the new address.
    po.shutdownFd = fd;

    // We may be in the last call to poServer_doNextEvent().
    // poServer_shutdown() which closes listener all listener fd's and then
    // calls _poTato_checkSignalNewServerContinue() which signals the
    // new server to continue via the UNIX domain socket by writing to
    // it.

     for(s = po.servers; s; s = s->next)
        s->isRunning = false;

    // We may accept one more "round of connections".  It depends where we
    // catch this signal.

    return;

fail:

    po.shutdownFd = -1;

    if(fd >= 0)
        close(fd);
}

// We can't leave the listening sockets open while the new server starts
// up, because the address/port bindings of the old server and the new
// server may not be able to co-exist, due to them having incompatible
// address/port bindings.  I think that having one process with socket
// bound to 0.0.0.0:3333 and another process with socket bound to
// 127.0.0.1:3333 is not possible.  Yup, just tested that case.
void _poTato_checkSignalNewServerContinue(void)
{
    if(po.shutdownFd == -1) return;

    struct POServer *s;
    for(s=po.servers; s; s=s->next)
        if(!s->listenersDown)
            break;

    if(!s)
    {
        char c = END_CHAR;
        // all servers have listeners closed, so now
        // we signal the new server program to continue.
        errno = 0;
        // TODO: We could just use the closing of the socket as
        // the signal, and not bother writing.
        writen(po.shutdownFd, &c, 1);
        close(po.shutdownFd);
        NLOG("signaled new server to continue startup\n");
        po.shutdownFd = -1;
    }
}

static void terminate(void)
{
    if(po.terminating) return;
    po.terminating = true;

    struct POServer *s;
    for(s = po.servers; s; s = s->next)
    {
        if(!s->isRunning) exit(1);
        s->isRunning = false;
    }
}

static void termHandler(int sig)
{
    WLOG("caught terminal signal=%d\n", sig);
    if(po.terminating)
        exit(1);
    terminate();
}

static void segvHandler(int sig)
{
    VASSERT(0, "caught signal = %d\n", sig);
}

// called from server_create()
bool _poTato_init(void)
{
    if(po.isInit) return false; // success

    // ignore signal SIGPIPE.  Keeps gnutls_bye() from
    // causing the program to get the SIGPIPE signal, making the
    // program terminate. Instead we will handle read/write errors.
    if(setSignal(SIGPIPE, SIG_IGN, NULL)) return true; // error
    if(setSignal(SIGUSR1, NULL, passListeners)) return true; // error
    if(setSignal(SIGUSR2, NULL, passListeners)) return true; // error
    if(setSignal(SIGSEGV, segvHandler, NULL)) return true; // error
    if(setSignal(SIGINT, termHandler, NULL)) return true; // error

    po.masterTid = gettid();
    po.isInit = true;
    return false;
}

static inline
unsigned short getPort(struct sockaddr *addr)
{
    if(addr->sa_family == AF_INET)
        return ((struct sockaddr_in *) addr)->sin_port;
    if(addr->sa_family == AF_INET6)
        return ((struct sockaddr_in6 *) addr)->sin6_port;
#ifdef PO_DEBUG
    else
        DVASSERT(0, "unknown socket address type\n");
#endif
    return 0; // error
}

char *_poTato_getAddrStr(const struct sockaddr *addr, char *buf, size_t len)
{
    char addr_buf[128];
    if(addr->sa_family == AF_INET)
    {
        struct sockaddr_in *a;
        a = (struct sockaddr_in *) addr;
        snprintf(buf, len, "%s:%d",
                inet_ntop(AF_INET, &a->sin_addr, addr_buf,
                sizeof(addr_buf)), ntohs(a->sin_port));
    }
    else if(addr->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *a;
        a = (struct sockaddr_in6 *) addr;
        snprintf(buf, len, "%s:%d\n",
                inet_ntop(AF_INET6, &a->sin6_addr, addr_buf,
                sizeof(addr_buf)), ntohs(a->sin6_port));
    }
    else
    {
        memset(buf, 0, len);
        DVASSERT(0, "unknown address family\n");
    }

    return buf;
}

void _poTato_handoffCleanup(void)
{
    if(!po.handoffSockets) return;

    uint32_t i;
    for(i=0; i<po.numListeners; ++i)
    {
        if(po.handoffSockets[i].fd >= 0)
        {
#ifdef PO_ELOG_DEBUG
            char buf[64];
            DLOG("Closing extra handoff listener at address=%s fd=%d\n",
                _poTato_getAddrStr(&po.handoffSockets[i].addr,
                    buf, sizeof(buf)),
                po.handoffSockets[i].fd);
#endif
            close(po.handoffSockets[i].fd);
        }
    }

    free(po.handoffSockets);
    po.handoffSockets = NULL;
    po.numListeners = 0;
}

// returns fd or -1 if not found
// This finds a listener fd, with a given sockaddr, that was passed from
// an old server through a UNIX domain socket via
// poTato_signalHandOff().
int _poTato_findListenerFd(struct sockaddr *addr)
{
    DASSERT(po.isInit);
    DASSERT(po.masterTid == gettid());

    uint32_t i;
    for(i=0; i<po.numListeners; ++i)
    {
        if(po.handoffSockets[i].fd == -1)
            continue;

        if(getPort(&po.handoffSockets[i].addr) == getPort(addr))
        {
            int fd;
#ifdef PO_ELOG_WARNING
            char buf1[128];
#endif
            if(memcmp(&po.handoffSockets[i].addr, addr, sizeof(*addr)) == 0)
            {
                NLOG("got listener socket fd=%d at address=%s from handoff\n",
                    po.handoffSockets[i].fd,
                    _poTato_getAddrStr(addr, buf1, sizeof(buf1)));
                fd = po.handoffSockets[i].fd;
            }
            else // same port, but not same address
            {
#ifdef PO_ELOG_WARNING
                char buf2[128];
#endif
                fd = -1;
                close(po.handoffSockets[i].fd);
                WLOG("handoff address=%s incompatible with"
                        " new address=%s will close/reopen\n",
                        _poTato_getAddrStr(&po.handoffSockets[i].addr,
                            buf1, sizeof(buf1)),
                        _poTato_getAddrStr(addr, buf2, sizeof(buf2)));
            }

            po.handoffSockets[i].fd = -1; // mark as used
            return fd;
        }
    }
    return -1; // not found
}

void _poTato_addServer(struct POServer *s)
{
    DASSERT(s);

    s->next = po.servers;
    s->prev = NULL;
    if(po.servers)
        po.servers->prev = s;
    po.servers = s;
}

void _poTato_removeServer(struct POServer *s)
{
    DASSERT(s);

    if(s->next)
    {
        s->next->prev = s->prev;
    }
    if(s->prev)
    {
        s->prev->next = s->next;
    }
    else
    {
        DASSERT(s == po.servers);
        po.servers = s->next;
    }
}

// Get listening socket file descriptors from an already running potato
// server via a UNIX domain socket, after signaling it with SIGUSR1,
// then waits for the other server to exit, via a failed read() on
// a closed UNIX domain socket.
bool poTato_signalHandOff(const char *pidstr, bool shutdownOldServer)
{
    DASSERT(pidstr);
    DASSERT(pidstr[0]);

    int sfd = -1/*server fd*/, fd = -1/*connection fd*/;
    pid_t pid;
    struct sockaddr_un addr;
    char path[64];
    uint32_t numListeners = 0, totalListeners, i;

    errno = 0;
    pid = strtol(pidstr, NULL, 10);
    if(VASSERT(errno == 0, "strtol(\"%s\", NULL, 10) failed\n", pidstr)
            || VASSERT(pid, "strtol(\"%s\", NULL, 10) failed\n", pidstr))
        return true; // error

    if(ASSERT((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0))
        return true;

    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path), HANDOFF_PREFIX "%d", pid);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path, sizeof(path));
    unlink(path);

    // This creates the file path for the UNIX domain socket.
    if(ASSERT(bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) == 0))
        goto fail;

    if(ASSERT(listen(sfd, 1) == 0))
        goto fail;

    // Signal the other server.
    if(ASSERT(kill(pid, shutdownOldServer?SIGUSR1:SIGUSR2) == 0))
        goto fail; // error

    if(ASSERT((fd = accept(sfd, NULL, NULL)) >= 0))
        goto fail;

    unlink(path);

    close(sfd);
    sfd = -1;
    // All that code just to get one stream file open.


    // TODO: this could hang forever if the other server does not write.
    if(readn(fd, &numListeners, sizeof(numListeners)))
        goto fail;

    totalListeners = numListeners + po.numListeners;

    //DLOG("got %"PRIu32" listeners\n", numListeners);

    // We can add sockets from more than one process.

    if(ASSERT(po.handoffSockets =
                realloc(po.handoffSockets,
                    sizeof(*(po.handoffSockets))*(totalListeners))))
        goto fail;

    for(i=po.numListeners; i<totalListeners; ++i)
        po.handoffSockets[i].fd = -1;

    for(i=po.numListeners; i<totalListeners; ++i)
    {
        if(readn(fd, &(po.handoffSockets[i].addr), sizeof(struct sockaddr)))
            goto fail;

        // TODO: check that all parts of binding address is the same,
        // for example not just the port, but the address too.

        if(ASSERT((po.handoffSockets[i].fd = recvFd(fd)) >=0))
            goto fail;

#ifdef PO_ELOG_INFO
        char addr_buf[128];
        ILOG("listening fd=%d %s\n", po.handoffSockets[i].fd,
                _poTato_getAddrStr(&(po.handoffSockets[i].addr),
                addr_buf, sizeof(addr_buf)));
#endif
    }

    po.numListeners += numListeners;


    if(!shutdownOldServer)
    {
        close(fd);
        WLOG("got %"PRIu32" listeners from server at pid=%d; "
                "server continuing startup\n",
            numListeners, pid);

        return false; // success
    }

    WLOG("got %"PRIu32" listeners from server at pid=%d; "
            "waiting for old server to signal\n",
            numListeners, pid);


    // This read() will block until the socket brakes when the old server
    // closes the other end.  Most of the time it appears to read()=0
    // which means the end-of-file, closed pipe thingy, but I could get a
    // broken pipe error too.  The only thing we count on is that the
    // read() call blocks until the other end closes.
    char c;
    if(readn(fd, &c, 1) || c != END_CHAR)
        goto fail;

    DLOG("old server signaled that it closed the listeners\n");

    close(fd);

    return false; // success

fail:

    if(sfd >= 0)
        close(sfd);
    if(fd >= 0)
        close(fd);

    if(numListeners && po.numListeners == 0 && po.handoffSockets)
    {
        free(po.handoffSockets);
        po.handoffSockets = NULL;
    }

    return true;
}

int poTato_removeChildren(void)
{
    // TODO: We need to block SIGCHLD signal at some points in here??
    // The value for po.childPids[i] may be crap.
    DASSERT(po.isInit);
    DASSERT(po.masterTid == gettid());
    errno = 0;

    if(!po.childPids)
    {
        DASSERT(po.numChildren == 0);
        ILOG("There were no forked children from this process\n");
        return 0; // there were no potato forked children
    }

    int i;
    for(i=po.numChildren-1;i>=0;--i)
    {
        pid_t rpid, pid;
        int status;

        pid = po.childPids[i];

        if(pid < 1)
            // This child appears to have exited already.
            continue;

        ILOG("signaling child pid %d to terminate\n", pid);

        if(kill(pid, SIGINT) != 0)
        {
            // nothing we can do if kill() failed
            // It may be that a shell already signaled it.
            ILOG("signaling child %d to terminate failed\n", pid);
            errno = 0;
        }

        if(po.childPids[i] < 1)
            // This child appears to have exited while
            // we were here.
            continue;

        do
        {
            errno = 0;
            status = 0;
            rpid = waitpid(pid, &status, 0);
        } while(rpid == -1 && errno == EINTR);

        if(rpid == pid && WEXITSTATUS(status) && po.childPids[i] > 0)
        {
            DASSERT(WIFEXITED(status));

            if(WEXITSTATUS(status))
            {
                po.exitStatus += WEXITSTATUS(status);
                if(po.exitStatus > 255)
                    po.exitStatus = 255;
                ILOG("child pid %d exited with status %d\n",
                        pid, WEXITSTATUS(status));
            }
        }

        po.childPids[i] = -1;
    }

    free(po.childPids);
    po.childPids = NULL;
    po.numChildren = 0;
    return po.exitStatus;
}

static inline
void removePidFromChildList(pid_t pid)
{
    int i;
    for(i=0; i<po.numChildren; ++i)
        if(po.childPids[i] == pid)
        {
            po.childPids[i] = -1;
            return;
        }
}

static void sigChildHandler(int sig, siginfo_t *si, void *context)
{
    DASSERT(si->si_pid);
    DASSERT(sig == SIGCHLD);

    errno = 0;
    ILOG("caught signal=SIGCHLD to child pid %d\n",
            si->si_pid);

    switch(si->si_code)
    {
        case CLD_KILLED:
        case CLD_DUMPED:
        case CLD_TRAPPED:
            // We can't add children's descendants statuses
            // because it did not get to run that code to get it.
            NLOG("child pid %d exited with abnormal error status\n",
                    si->si_pid);
            po.exitStatus += 1;

            if(po.exitStatus > 255)
                po.exitStatus = 255;
            removePidFromChildList(si->si_pid);
            // The potato server continues running with one less
            // child.  That could be what is wanted by the user.
            return;
        case CLD_EXITED:
            NLOG("child pid %d exited with normal error status\n",
                    si->si_pid);
            removePidFromChildList(si->si_pid);
            // The potato server continues running with one less
            // child.  That could be what is wanted by the user.
            return;
    }

#ifdef PO_ELOG_INFO
    if(si->si_code == CLD_STOPPED)
        ILOG("child pid %d stopped\n", si->si_pid);
    else if(si->si_code == CLD_CONTINUED)
        ILOG("child pid %d continued\n", si->si_pid);
#endif
}

// Make one more potato server child process.  Do not call this after
// processing any events.  The parent waits for the child using a pipe.
pid_t poTato_fork(void)
{
    DASSERT(po.shutdownFd == -1);
    DASSERT(po.isInit);
    DASSERT(po.masterTid == gettid());

    pid_t pid;
    int fd[2]; // pipe fd

    errno = 0;
    if(setSignal(SIGCHLD, NULL, sigChildHandler))
        return -1; // fail

    if(ASSERT(pipe(fd) == 0))
        return -1; // fail

    if(ASSERT((pid = fork()) != -1))
    {
        close(fd[0]);
        close(fd[1]);
        return pid; // error
    }

    if(pid)
    {
        ///////////////////////////////
        // I'm the parent
        ///////////////////////////////

        char buf;

        ILOG("parent waiting for child server to start\n");

        if(ASSERT(close(fd[1]) == 0)) // close pipe writer
            goto parentFail1;

        // wait for child's signal
        if(ASSERT(read(fd[0], &buf, 1) == 1) // blocking read
                || ASSERT(buf == 'c'))
            goto parentFail1;
        if(ASSERT(close(fd[0]) == 0)) // close pipe reader
            goto parentFail2;

        if(ASSERT(po.childPids = realloc(po.childPids,
                        sizeof(pid_t)*(++po.numChildren))))
            goto parentFail2;

        po.childPids[po.numChildren-1] = pid;

        // Important message.
        NLOG("parent resuming after successfully making "
                "new server process\n");
        return pid; // success

    parentFail1:
        close(fd[0]); // close pipe reader
    parentFail2:
        // kill the child by signal, so it may do a shutdown
        ILOG("Parent process will signal child after failing to make "
                "new server process\n");
        kill(pid, SIGINT);
        // This is not fatal for the parent process so WLOG() not ELOG().
        WLOG("parent resuming after failing to make new server process\n");
        return -1; // error
    }

    ///////////////////////////////
    // I'm the child
    ///////////////////////////////

    int i;
    char buf = 'c';
    struct POServer *s;

    ILOG("child starting new server process\n");
    DASSERT(pid == 0);
    DASSERT(po.servers);

    if(ASSERT(close(fd[0]) == 0)) // close pipe reader
        goto childFail2;

    for(s = po.servers; s; s = s->next)
    {
        DASSERT(s->efd >= -1);
        DASSERT(s->numListeners == PO_SERVER_NUM_LISTENERS);
        DASSERT(!s->waiting.first); // no listeners waiting
        DASSERT(!s->used.first); // no connections

        // close the old epoll
        if(ASSERT(close(s->efd) == 0))
        {
            s->efd = -1;
            goto childFail1;
        }
        // create a new epoll
        if(ASSERT((s->efd = epoll_create1(EPOLL_CLOEXEC)) >= 0))
            goto childFail1;

        // fill the epoll with all the listener sockets
        for(i = s->numListeners - 1; i >= 0; --i)
        {
            struct POServer_listener *l;
            l = &s->listener[i];
            DASSERT(l->fd >= 0);
            if(_poServer_addListenerToEpoll(s, l, l->fd))
                goto childFail1;
        }
    }

    // signal the parent with the pipe
    ILOG("child signaling parent via a pipe write\n");
    if(ASSERT(write(fd[1], &buf, 1) == 1))
        goto childFail1;
    if(ASSERT(close(fd[1]) == 0)) // close the pipe writer
        goto childFail2;

    // This child is a master thread in this process.
    po.masterTid = gettid();
    if(po.childPids)
    {
        // This new process has no children yet.
        DASSERT(po.numChildren);
        free(po.childPids);
        po.childPids = NULL;
        po.numChildren = 0;
    }
#ifdef PO_DEBUG
    else DASSERT(po.numChildren == 0);
#endif

    po.exitStatus = 0;

    // Important message.
    WLOG("child finished successfully making new server process\n");

    return pid; // success pid == 0

childFail1:
    close(fd[1]);

childFail2:
    ELOG("This child failed to make new server process\n");
    exit(1);
}

static inline
const char *_poTato_getOptString(size_t optlen, const char *opt, char ***argv)
{
    if(strcmp(**argv, opt) == 0 && *(*argv + 1))
    {
        // --opt val
        ++(*argv);
        return **argv;
    }
    if(strncmp(**argv, opt, optlen) == 0 && (**argv)[optlen] == '=' &&
            (**argv)[1+optlen])
        // --opt=val
        return &(**argv)[1+optlen];
    return NULL; // not found
}

bool poTato_doOptions(const char **argvv, int optflags,
        bool (*doOtherOpts)(const char *argv1, const char *argv2))
{
    char **argv;
    argv = (char **) argvv;

    while(*argv)
    {
        const char *s;

        if((optflags & PO_OPTION_FORK) &&
                (s = _poTato_getOptString(6, "--fork", &argv)))
            po.numForks = strtol(s, NULL, 10);
        else if((optflags & PO_OPTION_COPY_LISTENERS) &&
                (s = _poTato_getOptString(6, "--copy", &argv)))
        {
            if(poTato_signalHandOff(s, false/*shutdown old server*/))
                return true; // error
        }
        else if((optflags & PO_OPTION_REPLACE_LISTENERS) &&
                (s = _poTato_getOptString(9, "--replace", &argv)))
        {
            if(poTato_signalHandOff(s, true/*shutdown old server*/))
                return true; // error
        }
        else if(doOtherOpts)
        {
            if(doOtherOpts(*argv, *(argv + 1)))
                return true; // error
        }
        else
        {
            WLOG("Unknown option \"%s\"\n", *argv);
            return true; // error
        }
        ++argv;
    }

    if(po.numForks < 0)
        po.numForks = 0;

    return false; // success
}

bool poTato_doOptionsFork(void)
{
    NLOG("Will fork %d additional servers at startup\n", po.numForks);

    if(!po.numForks) return false; // nothing to do, success

    int numForks;
    numForks = po.numForks;
    while(numForks--)
    {
        pid_t pid;
        pid = poTato_fork();
        if(-1 == pid)
            return true; // error
        if(pid == 0) return false; // child success
    }

    return false; // success
}
