#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "clib.h"
#include "cnet.h"
#include "msg.h"

ssize_t sendbytes(int sock, char *buf, size_t count);
ssize_t recvbytes(int sock, char *buf, size_t count);

int main(int argc, char *argv[]) {
    int z;
    int s0;
    struct sockaddr sa;
    str_t *serveripaddr = str_new(0);

    if (argc < 3) {
        printf("Usage: tclient <server domain> <port>\n");
        printf("Ex. tclient 127.0.0.1 5000\n");
        exit(1);
    }

    char *server_host = argv[1];
    char *server_port = argv[2];

    s0 = open_connect_sock(server_host, server_port, &sa);
    if (s0 == -1) {
        print_error("open_connect_sock()");
        return 1;
    }
    get_ipaddr_string(&sa, serveripaddr);
    printf("Connected to %s:%s.\n", serveripaddr->s, server_port);

    char *skipchars = "some chars to skip some chars to skip some chars to skip some chars to skip";
    sendbytes(s0, skipchars, strlen(skipchars));

    TextMsg tm;
    tm.msgno = TEXTMSG_NO;
    strcpy(tm.alias, "rob");
    strcpy(tm.text, "This is a song that took me a long time to write...");

    char *msgbs = pack_msg(&tm);
    assert(msgbs != NULL);
    send(s0, msgbs, MSG_HEADER_LEN + TEXTMSG_LEN, 0);

    sleep(1);

    sendbytes(s0, skipchars, strlen(skipchars));
    strcpy(tm.text, "Life is a flower so precious in your hand...");
    msgbs = pack_msg(&tm);
    assert(msgbs != NULL);
    send(s0, msgbs, MSG_HEADER_LEN + TEXTMSG_LEN, 0);

    sleep(2);

    str_free(serveripaddr);
    close(s0);
    return 0;
}

// Send count buf bytes into sock.
// Returns num bytes sent or -1 for error
ssize_t sendbytes(int sock, char *buf, size_t count) {
    int nsent = 0;
    while (nsent < count) {
        int z = send(sock, buf+nsent, count-nsent, 0);
        printf("sendbytes() z: %d\n", z);
        // socket closed, no more data
        if (z == 0) {
            // socket closed
            break;
        }
        // interrupt occured during send, retry send.
        if (z == -1 && errno == EINTR) {
            continue;
        }
        // no data available at the moment, just return what we have.
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        // any other error
        if (z == -1) {
            return z;
        }
        nsent += z;
    }

    return nsent;
}

// Receive count bytes into buf.
// Returns num bytes received or -1 for error.
ssize_t recvbytes(int sock, char *buf, size_t count) {
    memset(buf, '*', count); // initialize for debugging purposes.

    int nread = 0;
    while (nread < count) {
        int z = recv(sock, buf+nread, count-nread, 0);
        // socket closed, no more data
        if (z == 0) {
            break;
        }
        // interrupt occured during read, retry read.
        if (z == -1 && errno == EINTR) {
            continue;
        }
        // no data available at the moment, just return what we have.
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        // any other error
        if (z == -1) {
            return z;
        }
        nread += z;
    }

    return nread;
}

