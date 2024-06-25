#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "clib.h"
#include "cnet.h"

// Number of bytes to send/recv at a time.
#define NET_BUFSIZE 512

// Reads up to nbytes into buffer.
// Returns one of the following:
//    1 (Z_OPEN) for socket open (socket data available)
//    0 (Z_EOF) for EOF
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket (no socket data available)
// On return, num_bytes_received contains the number of bytes read.
int recv_buf_bytes(int fd, buf_t *buf, size_t nbytes, size_t *num_bytes_received) {
    int z;
    char readbuf[NET_BUFSIZE];

    size_t nread = 0;
    while (nread < nbytes) {
        // read minimum of sizeof(readbuf) and nbytes-nread
        int nblock = nbytes-nread;
        if (nblock > sizeof(readbuf))
            nblock = sizeof(readbuf);

        z = recv(fd, readbuf, nblock, MSG_DONTWAIT);
        if (z == 0) {
            z = Z_EOF;
            break;
        }
        if (z == -1 && errno == EINTR) {
            continue;
        }
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            z = Z_BLOCK;
            break;
        }
        if (z == -1) {
            z = Z_ERR;
            break;
        }
        assert(z > 0);
        buf_append(buf, readbuf, z);
        nread += z;
    }
    if (z > 0) {
        z = Z_OPEN;
    }
    *num_bytes_received = nread;
    return z;
}

// Cumulatively reads socket bytes into buffer.
// Returns one of the following:
//    0 (Z_EOF) for EOF
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket (no data)
int recv_buf(int fd, buf_t *buf) {
    int z;
    char readbuf[NET_BUFSIZE];
    while (1) {
        z = recv(fd, readbuf, sizeof(readbuf), MSG_DONTWAIT);
        printf("recv_buf() z: %d\n", z);
        if (z == 0) {
            z = Z_EOF;
            break;
        }
        if (z == -1 && errno == EINTR) {
            continue;
        }
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            z = Z_BLOCK;
            break;
        }
        if (z == -1) {
            z = Z_ERR;
            break;
        }
        assert(z > 0);
        buf_append(buf, readbuf, z);
    }
    assert(z <= 0);
    return z;
}
// Cumulatively write buffer bytes into socket.
// Returns one of the following:
//    0 (Z_EOF) for all buf bytes sent
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket
int send_buf(int fd, buf_t *buf) {
    int z;
    while (1) {
        int nwrite = buf->len - buf->cur;
        if (nwrite <= 0) {
            z = Z_EOF;
            break;
        }
        z = send(fd,
                 buf->p + buf->cur, 
                 buf->len - buf->cur,
                 MSG_DONTWAIT | MSG_NOSIGNAL);
        if (z == -1 && errno == EINTR) {
            continue;
        }
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            z = Z_BLOCK;
            break;
        }
        if (z == -1) {
            z = Z_ERR;
            break;
        }
        assert(z >= 0);
        buf->cur += z;
    }
    assert(z <= 0);
    return z;
}

// Return new socket fd for listening or -1 for error.
int open_listen_sock(char *host, char *port, int backlog, struct sockaddr *psa) {
    int z;

    struct addrinfo hints, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    z = getaddrinfo(host, port, &hints, &ai);
    if (z != 0) {
        printf("getaddrinfo(): %s\n", gai_strerror(z));
        errno = EINVAL;
        return -1;
    }
    if (psa != NULL) {
        memcpy(psa, ai->ai_addr, ai->ai_addrlen);
    }

    int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd == -1) {
        print_error("socket()");
        z = -1;
        goto error_return;
    }
    int yes=1;
    z = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (z == -1) {
        print_error("setsockopt()");
        goto error_return;
    }
    z = bind(fd, ai->ai_addr, ai->ai_addrlen);
    if (z == -1) {
        print_error("bind()");
        goto error_return;
    }
    z = listen(fd, backlog);
    if (z == -1) {
        print_error("listen()");
        goto error_return;
    }

    freeaddrinfo(ai);
    return fd;

error_return:
    freeaddrinfo(ai);
    return z;
}
// Return new socket fd for sending/receiving or -1 for error.
// You can specify the host and port in two ways:
// 1. host="domain.xyz", port="5001" (separate host and port)
// (#2 not working at the moment)
// 2. host="domain.xyz:5001", port="" (combine host:port in host parameter)
int open_connect_sock(char *host, char *port, struct sockaddr *psa) {
    int z;

    struct addrinfo hints, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    z = getaddrinfo(host, port, &hints, &ai);
    if (z != 0) {
        printf("getaddrinfo(): %s\n", gai_strerror(z));
        errno = EINVAL;
        return -1;
    }
    if (psa != NULL) {
        memcpy(psa, ai->ai_addr, ai->ai_addrlen);
    }

    int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd == -1) {
        print_error("socket()");
        z = -1;
        goto error_return;
    }
    int yes=1;
    z = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (z == -1) {
        print_error("setsockopt()");
        goto error_return;
    }
    z = connect(fd, ai->ai_addr, ai->ai_addrlen);
    if (z == -1) {
        print_error("connect()");
        goto error_return;
    }

    freeaddrinfo(ai);
    return fd;

error_return:
    freeaddrinfo(ai);
    return z;
}
void set_sock_timeout(int sock, int nsecs, int ms) {
    struct timeval tv;
    tv.tv_sec = nsecs;
    tv.tv_usec = ms * 1000; // convert milliseconds to microseconds
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
}
void set_sock_nonblocking(int sock) {
    fcntl(sock, F_SETFL, O_NONBLOCK);
}
// Return sin_addr or sin6_addr depending on address family.
static void *sockaddr_sin_addr(struct sockaddr *sa) {
    // addr->ai_addr is either struct sockaddr_in* or sockaddr_in6* depending on ai_family
    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *p = (struct sockaddr_in*) sa;
        return &(p->sin_addr);
    } else {
        struct sockaddr_in6 *p = (struct sockaddr_in6*) sa;
        return &(p->sin6_addr);
    }
}
// Return sin_port or sin6_port depending on address family.
unsigned short get_sockaddr_port(struct sockaddr *sa) {
    // addr->ai_addr is either struct sockaddr_in* or sockaddr_in6* depending on ai_family
    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *p = (struct sockaddr_in*) sa;
        return ntohs(p->sin_port);
    } else {
        struct sockaddr_in6 *p = (struct sockaddr_in6*) sa;
        return ntohs(p->sin6_port);
    }
}
// Return human readable IP address from sockaddr
void get_ipaddr_string(struct sockaddr *sa, str_t *ipaddr) {
    char servipstr[INET6_ADDRSTRLEN];
    const char *pz = inet_ntop(sa->sa_family, sockaddr_sin_addr(sa),
                               servipstr, sizeof(servipstr));
    if (pz == NULL) {
        print_error("inet_ntop()");
        str_assign(ipaddr, "");
        return;
    }
    str_assign(ipaddr, servipstr);
}

