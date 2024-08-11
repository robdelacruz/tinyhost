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

#define NREADBYTES 32

enum RecvState {
    RECV_SIG,
    RECV_HEADER,
    RECV_BODY
};

typedef struct {
    int fd;
    buf_t *readbuf;
    enum RecvState recvstate;
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
array_t *_received_msgs;

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

    _ctxs = array_new(0, (voidpfunc_t) clientctx_free);
    _received_msgs = array_new(0, (voidpfunc_t) free_msg);

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

            buf_t *readbuf = ctx->readbuf;

            while (1) {
                z = recv_buf(readfd, readbuf, NREADBYTES, NULL);
                if (z == Z_ERR) {
                    print_error("recv_bytes()");
                }
                if (z == Z_EOF) {
                    disconnect_client(readfd);
                    break;
                }

                if (ctx->recvstate == RECV_SIG) {
                    if (readbuf->len < MSG_SIG_LEN)
                        break;

                    if (strncmp(readbuf->p, MSG_SIG, MSG_SIG_LEN) != 0) {
                        printf("Invalid header sig in message.\n");
                        disconnect_client(readfd);
                    }
                    ctx->recvstate = RECV_HEADER;
                } else if (ctx->recvstate == RECV_HEADER) {
                    if (readbuf->len < MSG_HEADER_LEN)
                        break;

                    short bodylen = ntohs(*MSG_OFFSET_BODYLEN(readbuf->p));
                    if (bodylen > MSG_MAX_BODYLEN) {
                        printf("Invalid bodylen in message (bodylen: %d)\n", bodylen);
                        ctx->recvstate = RECV_SIG;
                        disconnect_client(readfd);
                        break;
                    }
                    ctx->recvstate = RECV_BODY;
                } else if (ctx->recvstate == RECV_BODY) {
                    short bodylen = ntohs(*MSG_OFFSET_BODYLEN(readbuf->p));
                    assert(bodylen <= MSG_MAX_BODYLEN);
                    int msglen = MSG_HEADER_LEN + bodylen;

                    // Received entire message
                    if (readbuf->len >= msglen) {
                        void *msg = unpack_msg_bytes(readbuf->p);
                        if (msg)
                            array_add(_received_msgs, msg);

                        short msgno = MSGNO(msg);
                        printf("Received message (msgno: %d)\n", msgno);

                        if (msgno == TEXTMSG_NO) {
                            TextMsg *tm = (TextMsg *) msg;
                            printf("TextMsg - alias: '%s', text: '%s'\n", tm->alias, tm->text);
                        }

                        // Move extra received bytes into start of readbuf.
                        int nleftover = readbuf->len - msglen;
                        memcpy(readbuf->p, readbuf->p + msglen, nleftover);
                        readbuf->len = nleftover;
                        memset(readbuf->p + readbuf->len, 0, readbuf->cap - readbuf->len);

                        ctx->recvstate = RECV_SIG;
                    }
                }

            }
        } // for _maxfd

    } // while (1)

    str_free(serveripaddr);
    close(s0);
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
    ctx->readbuf = buf_new(0);
    ctx->recvstate = RECV_SIG;
    return ctx;
}
void clientctx_free(clientctx_t *ctx) {
    buf_free(ctx->readbuf);
    free(ctx);
}
void clientctx_reset(clientctx_t *ctx) {
    buf_clear(ctx->readbuf);
    ctx->recvstate = RECV_SIG;
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

