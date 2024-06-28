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
// TH/0.1 login\n
// hostname: rob\n
// password: tiny\n
// \n
//
// tinyhost/0.1 logout\n
// \n
//
// TH/0.1 send message\n
// hostname: guest1\n
// body-length: 12\n
// \n
// Hello guest1
//

typedef enum {
    READING_HEAD=0,
    READING_ARGS,
    READING_BODY,
} msgstate_t;

typedef struct {
    int fd;
    buf_t *buf;
    str_t *ver;
    str_t *cmd;
    array_t *args;
    buf_t *body;
    int body_len;
    msgstate_t msgstate;
} clientctx_t;

clientctx_t *clientctx_new(int fd);
void clientctx_free(clientctx_t *ctx);
void clientctx_set_arg(clientctx_t *ctx, char *arg_key, char *arg_val);
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
    str_t *strline = str_new(0);
    int hasline = 0;
    int hasbody = 0;

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

                while (1) {
                    if (ctx->msgstate == READING_HEAD) {
                        z = recv_line(readfd, ctx->buf, 0, strline, &hasline);
                        if (z == Z_ERR) {
                            print_error("recv_line()");
                        }
                        if (z == Z_EOF) {
                            FD_CLR(readfd, &_readfds);
                            shutdown(readfd, SHUT_RD);
                        }
                        if (!hasline)
                            break;

                        str_assign(ctx->cmd, strline->s);
                        str_assign(strline, "");
                        ctx->msgstate = READING_ARGS;
                        printf("cmd: '%s'\n", ctx->cmd->s);
                        continue;
                    }
                    if (ctx->msgstate == READING_ARGS) {
                        z = recv_line(readfd, ctx->buf, 0, strline, &hasline);
                        if (z == Z_ERR) {
                            print_error("recv_line()");
                        }
                        if (z == Z_EOF) {
                            FD_CLR(readfd, &_readfds);
                            shutdown(readfd, SHUT_RD);
                        }
                        if (!hasline)
                            break;

                        // Empty line read, no more args.
                        if (strline->len == 0) {
                            ctx->msgstate = READING_BODY;
                            printf("reading_args empty line buf->len: %ld\n", ctx->buf->len);
                            continue;
                        }

                        // Read one arg line: "key: val"
                        char *k = strline->s;
                        char *v = strchr(strline->s, ':');
                        if (v == NULL)
                            continue;

                        // v points to ':', move to first char of val
                        *v = '\0';
                        v++;
                        while (*v == ' ') {
                            *v = '\0';
                            v++;
                        }
                        clientctx_set_arg(ctx, k, v);
                        printf("READING_ARGS k: '%s', v: '%s'\n", k, v);

                        if (strcmp(k, "body-length") == 0)
                            ctx->body_len = atoi(v);
                        continue;
                    }
                    if (ctx->msgstate == READING_BODY) {
                        if (ctx->body_len == 0) {
                            //todo: process ctx->cmd
                            printf("command received:\n");
                            printf("head: '%s'\n", ctx->cmd->s);
                            for (int i=0; i < ctx->args->len; i++) {
                                array_t *arg = ctx->args->items[i];
                                assert(arg->len == 2);
                                str_t *k = arg->items[0];
                                str_t *v = arg->items[1];
                                printf("arg %d [%s] => [%s]\n", i, k->s, v->s);
                            }
                            buf_append(ctx->body, "\0", 1);
                            printf("body_len: %d\n", ctx->body_len);
                            printf("body: %s\n", ctx->body->p);
                                    
                            ctx->msgstate = READING_HEAD;
                            continue;
                        }
                        z = recv_bytes(readfd, ctx->buf, 0, ctx->body_len, ctx->body, &hasbody);
                        if (z == Z_ERR) {
                            print_error("recv_buf()");
                        }
                        if (z == Z_EOF) {
                            FD_CLR(readfd, &_readfds);
                            shutdown(readfd, SHUT_RD);
                        }
                        if (!hasbody)
                            break;

                        //todo: process ctx->cmd
                        printf("command with body received:\n");
                        printf("head: '%s'\n", ctx->cmd->s);
                        for (int i=0; i < ctx->args->len; i++) {
                            array_t *arg = ctx->args->items[i];
                            assert(arg->len == 2);
                            str_t *k = arg->items[0];
                            str_t *v = arg->items[1];
                            printf("arg %d [%s] => [%s]\n", i, k->s, v->s);
                        }
                        buf_append(ctx->body, "\0", 1);
                        printf("body_len: %d\n", ctx->body_len);
                        printf("body: %s\n", ctx->body->p);

                        ctx->msgstate = READING_HEAD;
                        continue;
                    }
                } // while (1)
            }
        } // for _maxfd

        // todo: Loop through _ctxs to see which has msgstate == MSG_COMPLETE
        //       and execute the command.

    } // while (1)

    str_free(serveripaddr);
    str_free(strline);
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
    ctx->args = array_new(0, (voidpfunc_t) array_free);
    ctx->body = buf_new(0);
    ctx->body_len = 0;
    ctx->msgstate = READING_HEAD;
    return ctx;
}
void clientctx_free(clientctx_t *ctx) {
    buf_free(ctx->buf);
    str_free(ctx->cmd);
    buf_free(ctx->body);
    free(ctx);
}
void clientctx_set_arg(clientctx_t *ctx, char *arg_key, char *arg_val) {
    array_t *kv;
    str_t *k, *v;

    for (int i=0; i < ctx->args->len; i++) {
        kv = (array_t *) ctx->args->items[i];
        assert(kv->len == 2);
        k = kv->items[0];
        v = kv->items[1];

        // Overwrite arg if it already exists.
        if (strcmp(k->s, arg_key) == 0) {
            str_assign(k, arg_key);
            str_assign(v, arg_val);
            return;
        }
    }
    // Add new arg.
    k = str_new_assign(arg_key);
    v = str_new_assign(arg_val);
    kv = array_new(2, (voidpfunc_t) str_free);
    array_add(kv, k);
    array_add(kv, v);
    array_add(ctx->args, kv);
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


