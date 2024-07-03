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
#include "msg.h"

typedef enum {
    HEAD=0,
    BODY,
} readstate_t;

typedef struct {
    int fd;
    buf_t *buf;
    buf_t *outbuf;

    readstate_t readstate;
    short msgver;
    char msgno;
    int body_len;
} clientctx_t;

void handle_sigint(int sig);
void handle_sigchld(int sig);

void disconnect_client(int fd);

clientctx_t *clientctx_new(int fd);
void clientctx_free(clientctx_t *ctx);
void clientctx_reset(clientctx_t *ctx);
clientctx_t *find_clientctx(array_t *ctxs, int fd);
void delete_clientctx(array_t *ctxs, int fd);

void print_buf(buf_t *buf);

fd_set _readfds;
int _maxfd=0;
array_t *_ctxs;

int main(int argc, char *argv[]) {
    int z;
    int s0;
    struct sockaddr sa;
    char *hostname = "localhost";
    char *port = "8001";
    str_t *serveripaddr = str_new(0);
    int complete = 0;

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

    _ctxs = array_new(0, (voidpfunc_t) clientctx_free);

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

            // New connection
            if (i == s0) {
                socklen_t sa_len = sizeof(struct sockaddr_in);
                struct sockaddr_in sa;
                int clientfd = accept(s0, (struct sockaddr*)&sa, &sa_len);
                if (clientfd == -1) {
                    print_error("accept()");
                    continue;
                }
                // Add client socket to readfds list.
                FD_SET(clientfd, &_readfds);
                if (clientfd > _maxfd)
                    _maxfd = clientfd;

                clientctx_t *ctx = clientctx_new(clientfd);
                array_add(_ctxs, ctx);

                printf("new clientfd: %d\n", clientfd);
                continue;
            }

            // Client socket data available to read.
            int readfd = i;
            //printf("read data from clientfd %d\n", readfd);

            clientctx_t *ctx = find_clientctx(_ctxs, readfd);
            assert(ctx != NULL);

            while (1) {
                if (ctx->readstate == HEAD) {
                    z = recv_bytes(readfd, ctx->buf, 0, MSGHEAD_SIZE, ctx->outbuf, &complete);
                    if (z == Z_ERR) {
                        print_error("recv_line()");
                    }
                    if (z == Z_EOF) {
                        disconnect_client(readfd);
                        break;
                    }
                    if (!complete)
                        break;

                    assert(ctx->outbuf->len == 4);

                    // Validate and parse received message head.
                    if (!read_msghead(ctx->outbuf->p, &ctx->msgver, &ctx->msgno)) {
                        printf("Message head invalid (msgno: %d, ver: %d).\n", ctx->msgno, ctx->msgver);
                        disconnect_client(readfd);
                        clientctx_reset(ctx);
                        break;
                    }

                    ctx->body_len = msgbody_bytes_size(ctx->msgno, ctx->msgver);
                    ctx->readstate = BODY;
                    continue;
                }
                if (ctx->readstate == BODY) {
                    if (ctx->body_len > 0) {
                        z = recv_bytes(readfd, ctx->buf, 0, ctx->body_len, ctx->outbuf, &complete);
                        if (z == Z_ERR) {
                            print_error("recv_line()");
                        }
                        if (z == Z_EOF) {
                            disconnect_client(readfd);
                            clientctx_reset(ctx);
                            break;
                        }
                        if (!complete)
                            break;

                        assert(isvalid_msgno(ctx->msgno, ctx->msgver));
                        BaseMsg *msg = create_msg(ctx->msgno, ctx->msgver);
                        assert(msg != NULL);

                        clientctx_reset(ctx);
                        continue;
                    }
                }
            }
        } // for _maxfd

    } // while (1)

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

void disconnect_client(int fd) {
    FD_CLR(fd, &_readfds);
    shutdown(fd, SHUT_RDWR);
    delete_clientctx(_ctxs, fd);
    printf("Disconnected client %d\n", fd);
}

clientctx_t *clientctx_new(int fd) {
    clientctx_t *ctx = malloc(sizeof(clientctx_t));
    ctx->fd = fd;
    ctx->buf = buf_new(0);
    ctx->outbuf = buf_new(0);

    ctx->readstate = HEAD;
    ctx->msgver = 0;
    ctx->msgno = 0;
    ctx->body_len = 0;
    return ctx;
}
void clientctx_free(clientctx_t *ctx) {
    buf_free(ctx->buf);
    buf_free(ctx->outbuf);
    free(ctx);
}
void clientctx_reset(clientctx_t *ctx) {
    ctx->readstate = HEAD;
    ctx->msgver = 0;
    ctx->msgno = 0;
    ctx->body_len = 0;
    buf_clear(ctx->outbuf);
}
clientctx_t *find_clientctx(array_t *ctxs, int fd) {
    for (int i=0; i < ctxs->len; i++) {
        clientctx_t *ctx = (clientctx_t *) ctxs->items[i];
        if (ctx->fd == fd)
            return ctx;
    }
    return NULL;
}
void delete_clientctx(array_t *ctxs, int fd) {
    for (int i=0; i < _ctxs->len; i++) {
        clientctx_t *ctx = _ctxs->items[i];
        if (ctx->fd == fd) {
            array_del(_ctxs, i);
            break;
        }
    }
}

void print_buf(buf_t *buf) {
    printf("buf (%ld bytes):", buf->len);
    for (int i=0; i < buf->len; i++) {
        printf("%c", buf->p[i]);
    }
    printf("\n");
}

