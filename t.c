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

// Message format:
//
// tinyhost/0.1 login\n
// hostname: rob\n
// password: tiny\n
// \n
//
// tinyhost/0.1 logout\n
// \n
//
// tinyhost/0.1 send message\n
// hostname: guest1\n
// body-length: 12\n
// \n
// Hello guest1
//

typedef enum {
    READING_CMD=0,
    READING_ARGS,
    READING_BODY,
    READING_COMPLETE,
} msgstate_t;

typedef struct {
    int fd;
    buf_t *buf;
    str_t *cmd;
    buf_t *body;
    msgstate_t msgstate;
} clientctx_t;

clientctx_t *clientctx_new(int fd);
void clientctx_free(clientctx_t *ctx);
clientctx_t *find_clientctx(array_t *ctxs, int fd);
void delete_clientctx(array_t *ctxs, int fd);

void handle_sigint(int sig);
void handle_sigchld(int sig);

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

    _ctxs = array_new(0, (voidpfunc_t)clientctx_free);

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
                clientctx_t *ctx = clientctx_new(clientfd);
                array_add(_ctxs, ctx);

                printf("new clientfd: %d\n", clientfd);
                continue;
            } else {
                // Client socket data available to read.
                int readfd = i;
                printf("read data from clientfd %d\n", readfd);

                clientctx_t *ctx = find_clientctx(_ctxs, readfd);
                assert(ctx != NULL);

                int hasline=0;
                if (ctx->msgstate == READING_CMD) {
                    z = recv_line(readfd, ctx->buf, 0, ctx->cmd, &hasline);
                    if (z == Z_ERR) {
                        print_error("recv_line()");
                    }
                    if (z == Z_EOF) {
                        FD_CLR(readfd, &_readfds);
                        shutdown(readfd, SHUT_RD);
                    }
                    if (hasline) {
                        ctx->msgstate = READING_ARGS;
                    }
                    continue;
                }
                if (ctx->msgstate == READING_ARGS) {
                    str_t *argline = str_new(0);
                    z = recv_line(readfd, ctx->buf, 0, argline, &hasline);
                    if (z == Z_ERR) {
                        print_error("recv_line()");
                    }
                    if (z == Z_EOF) {
                        FD_CLR(readfd, &_readfds);
                        shutdown(readfd, SHUT_RD);
                    }
                    if (hasline) {
                        if (argline->len == 0) {
                            // if body_length arg > 0, READING_COMPLETE
                            ctx->msgstate = READING_BODY;
                        } else {
                            ctx->msgstate == READING_ARGS;
                            // todo: read arg from argline
                        }
                    }
                    continue;
                }
                if (ctx->msgstate == READING_BODY) {
                    int body_len = 1; // todo: read body_length arg
                    // todo: use recv_buf_buflen() or something like
                    // that to receive max limit bytes in buf.
                    z = recv_buf(readfd, ctx->buf, body_len, NULL);
                    if (z == Z_ERR) {
                        print_error("recv_buf()");
                    }
                    if (z == Z_EOF) {
                        FD_CLR(readfd, &_readfds);
                        shutdown(readfd, SHUT_RD);
                    }
                }
            }
        } // for _maxfd

        // todo: Loop through _ctxs to see which has msgstate == MSG_COMPLETE
        //       and execute the command.

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

clientctx_t *clientctx_new(int fd) {
    clientctx_t *ctx = malloc(sizeof(clientctx_t));
    ctx->fd = fd;
    ctx->buf = buf_new(0);
    ctx->cmd = str_new(0);
    ctx->body = buf_new(0);
    ctx->msgstate = READING_CMD;
    return ctx;
}
void clientctx_free(clientctx_t *ctx) {
    buf_free(ctx->buf);
    str_free(ctx->cmd);
    buf_free(ctx->body);
    free(ctx);
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


