// From: 
// http://stackoverflow.com/questions/28003921/sending-file-descriptor-by-linux-socket/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>




void wyslij(int socket, int fd)  // send fd by socket
{
    struct msghdr msg = { 0 };
    char buf[CMSG_SPACE(sizeof(fd))];
    memset(buf, '\0', sizeof(buf));

    /* On Mac OS X, the struct iovec is needed, even if it points to minimal data */
    struct iovec io = { .iov_base = "", .iov_len = 1 };

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    struct cmsghdr * cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));

    memmove(CMSG_DATA(cmsg), &fd, sizeof(fd));

    msg.msg_controllen = cmsg->cmsg_len;

    if (sendmsg(socket, &msg, 0) < 0)
        err_syserr("Failed to send message\n");
}

int odbierz(int socket)  // receive fd from socket
{
    struct msghdr msg = {0};

    /* On Mac OS X, the struct iovec is needed, even if it points to minimal data */
    char m_buffer[1];
    struct iovec io = { .iov_base = m_buffer, .iov_len = sizeof(m_buffer) };
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    char c_buffer[256];
    msg.msg_control = c_buffer;
    msg.msg_controllen = sizeof(c_buffer);

    if (recvmsg(socket, &msg, 0) < 0)
        err_syserr("Failed to receive message\n");

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);

    err_remark("About to extract fd\n");
    int fd;
    memmove(&fd, CMSG_DATA(cmsg), sizeof(fd));
    err_remark("Extracted fd %d\n", fd);

    return fd;
}

