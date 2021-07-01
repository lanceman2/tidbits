#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stropts.h>
#include <stdio.h>

void
statout(char *f, struct stat *s)
{
	printf("stat: from=%s mode=0%o, ino=%ld, dev=%lx, rdev=%lx\n",
		f, s->st_mode, s->st_ino, s->st_dev, s->st_rdev);
	fflush(stdout);
}

void
sendfd(int p)
{
	int tfd;

	tfd = open(TESTFILE, O_RDWR);
	ioctl(p, I_SENDFD, tfd);
}

void
recvfd(int p)
{
	struct strrecvfd rfdbuf;
	struct stat statbuf;
	char			fdbuf[32];

	ioctl(p, I_RECVFD, &rfdbuf);
	fstat(rfdbuf.fd, &statbuf);
	sprintf(fdbuf, "recvfd=%d", rfdbuf.fd);
	statout(fdbuf, &statbuf);	
}


#define TESTFILE "/dev/null"
main(int argc, char *argv[])
{
	int fd;
	int pipefd[2];
	struct stat statbuf;

	stat(TESTFILE, &statbuf);
	statout(TESTFILE, &statbuf);
	pipe(pipefd);
	if (fork() == 0) {
		close(pipefd[0]);
		sendfd(pipefd[1]);
	} else {
		close(pipefd[1])
		recvfd(pipefd[0]);
	}
}





#include "apue.h"
#include <stropts.h>

/*
 * Pass a file descriptor to another process.
 * If fd<0, then -fd is sent back instead as the error status.
 */
int
send_fd(int fd, int fd_to_send)
{
    char    buf[2];     /* send_fd()/recv_fd() 2-byte protocol */
    
    buf[0] = 0;         /* null byte flag to recv_fd() */
    if (fd_to_send < 0) {
        buf[1] = -fd_to_send;   /* nonzero status means error */
        if (buf[1] == 0)
            buf[1] = 1; /* -256, etc. would screw up protocol */
    } else {
        buf[1] = 0;     /* zero status means OK */
    }

    if (write(fd, buf, 2) != 2)
        return(-1);
    if (fd_to_send >= 0)
        if (ioctl(fd, I_SENDFD, fd_to_send) < 0)
            return(-1);
    return(0);
}


#include "apue.h"
#include <stropts.h>

/*
 * Receive a file descriptor from another process (a server).
 * In addition, any data received from the server is passed
 * to (*userfunc)(STDERR_FILENO, buf, nbytes). We have a
 * 2-byte protocol for receiving the fd from send_fd().
 */
int
recv_fd(int fd, ssize_t (*userfunc)(int, const void *, size_t))
{
    int                 newfd, nread, flag, status;
    char                *ptr;
    char                buf[MAXLINE];
    struct strbuf       dat;
    struct strrecvfd    recvfd;

    status = -1;
    for ( ; ; ) {
        dat.buf = buf;
        dat.maxlen = MAXLINE;
        flag = 0;
        if (getmsg(fd, NULL, &dat, &flag) < 0)
            err_sys("getmsg error");
        nread = dat.len;
        if (nread == 0) {
            err_ret("connection closed by server");
            return(-1);
        }
        /*
         * See if this is the final data with null & status.
         * Null must be next to last byte of buffer, status
         * byte is last byte. Zero status means there must
         * be a file descriptor to receive.
         */
        for (ptr = buf; ptr < &buf[nread]; ) {
            if (*ptr++ == 0) {
                if (ptr != &buf[nread-1])
                    err_dump("message format error");
                 status = *ptr & 0xFF;   /* prevent sign extension */
                 if (status == 0) {
                     if (ioctl(fd, I_RECVFD, &recvfd) < 0)
                         return(-1);
                     newfd = recvfd.fd;  /* new descriptor */
                 } else {
                     newfd = -status;
                 }
                 nread -= 2;
            }
        }
        if (nread > 0)
            if ((*userfunc)(STDERR_FILENO, buf, nread) != nread)
                 return(-1);

        if (status >= 0)    /* final data has arrived */
            return(newfd);  /* descriptor, or -status */
    }
}

