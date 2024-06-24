#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include "clib.h"
#include "cnet.h"

typedef struct {
    int fd;
    buf_t *buf;
} netpipe_t;

void handle_sigint(int sig);
void handle_sigchld(int sig);

netpipe_t *netpipe_new(int fd);
void netpipe_free(netpipe_t *np);
netpipe_t *find_netpipe(array_t *nps, int fd);

fd_set _readfds;
int _maxfd=0;
array_t *_readpipes;

int main(int argc, char *argv[]) {
    int z;
    int s0;
    struct sockaddr sa;
    char *hostname = "localhost";
    char *port = "8001";
    str_t *serveripaddr = str_new(0);

    signal(SIGPIPE, SIG_IGN);           // Don't abort on SIGPIPE
    signal(SIGINT, handle_sigint);      // exit on CTRL-C
    signal(SIGCHLD, handle_sigchld);

    s0 = open_listen_sock(hostname, port, 50, &sa);
    if (s0 == -1) {
        print_error("open_listen_sock()");
        return 1;
    }
    get_ipaddr_string(&sa, serveripaddr);
    printf("Listening on %s port %s...\n", serveripaddr->s, port);

    FD_ZERO(&_readfds);
    FD_SET(s0, &_readfds);
    _maxfd = s0;

    _readpipes = array_new(0, (voidpfunc_t)netpipe_free);

    while (1) {
        fd_set readfds = _readfds;
        z = select(_maxfd+1, &readfds, NULL, NULL, NULL);
        if (z == -1 && errno == EINTR)
            continue;
        if (z == -1) {
            print_error("select()");
            return 1;
        }
        if (z == 0) // timeout
            continue;

        for (int i=0; i <= _maxfd; i++) {
            if (!FD_ISSET(i, &readfds))
                continue;

            // New connection, add client socket to readfds list.
            if (i == s0) {
                socklen_t sa_len = sizeof(struct sockaddr_in);
                struct sockaddr_in sa;
                int clientfd = accept(s0, (struct sockaddr*)&sa, &sa_len);
                if (clientfd == -1) {
                    print_error("accept()");
                    continue;
                }
                // Add client socket to readfds list.
                FD_SET(clientfd, &readfds);
                if (clientfd > _maxfd)
                    _maxfd = clientfd;

                // Add new client pipe
                netpipe_t *np = netpipe_new(clientfd);
                array_add(_readpipes, np);

                printf("new clientfd: %d\n", clientfd);
                continue;
            } else {
                // Client socket data available to read.
                int readfd = i;
                printf("read data from clientfd %d\n", readfd);

                netpipe_t *np = find_netpipe(_readpipes, readfd);
                assert(np != NULL);

                z = recv_buf(readfd, np->buf);
                printf("z: %d\n", z);
                if (z == Z_ERR)
                    print_error("recv_buf()");
                if (z == Z_EOF) {
                    buf_append(np->buf, "\0", 1);
                    printf("Received from clientfd %d:\n%s\n", readfd, np->buf->p);

                    for (int i=0; i < _readpipes->len; i++) {
                        netpipe_t *np = _readpipes->items[i];
                        if (np->fd == readfd) {
                            array_del(_readpipes, i);
                            break;
                        }
                    }
                    FD_CLR(readfd, &_readfds);
                    shutdown(readfd, SHUT_RD);
                }
            }
        }
    }

    str_free(serveripaddr);
    return 0;
}

void handle_sigint(int sig) {
    printf("SIGINT received\n");
    fflush(stdout);
    exit(0);
}

void handle_sigchld(int sig) {
    int tmp_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    errno = tmp_errno;
}

netpipe_t *netpipe_new(int fd) {
    netpipe_t *np = malloc(sizeof(netpipe_t));
    np->fd = fd;
    np->buf = buf_new(0);
    return np;
}
void netpipe_free(netpipe_t *np) {
    buf_free(np->buf);
    free(np);
}
netpipe_t *find_netpipe(array_t *nps, int fd) {
    for (int i=0; i < nps->len; i++) {
        netpipe_t *np = (netpipe_t *) nps->items[i];
        if (np->fd == fd)
            return np;
    }
    return NULL;
}


