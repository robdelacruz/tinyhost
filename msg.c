#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <arpa/inet.h>
#include "msg.h"

// Message head format:
// [0-1] TH (2 bytes, ascii)
// [2-3] version (2 bytes, 1-65535)
// [4] msgnum (1 byte, 1-255)
// [5...] message body
int read_msghead(char *msghead, short *ver, char *msgno) {
    if (strncmp(msghead, "TH", 2) != 0)
        return 0;

    *ver = ntohs(msghead[2]);
    *msgno = msghead[4];
    return isvalid_msgno(*msgno, *ver);
}
int isvalid_msgno(char msgno, short ver) {
    if (ver != 1)
        return 0;

    switch (msgno) {
    case LOGIN:
    case LOGOUT:
    case SEND_TEXT:
        return 1;
    }
    return 0;
}

int msgbody_bytes_size(char msgno, short ver) {
    if (ver != 1)
        return 0;
    if (msgno == LOGIN)
        return 35;
    else if (msgno == LOGOUT)
        return 0;
    else if (msgno == SEND_TEXT)
        return 90;
    else
        return 0;
}

static size_t msg_structsize(char msgno, short ver) {
    if (ver != 1)
        return 0;
    if (msgno == LOGIN)
        return sizeof(LoginMsg);
    else if (msgno == LOGOUT)
        return sizeof(LogoutMsg);
    else if (msgno == SEND_TEXT)
        return sizeof(SendTextMsg);
    else
        return 0;
}
void *create_msg(char msgno, short ver) {
    BaseMsg *msg;
    size_t msgsize = msg_structsize(msgno, ver);
    assert(msgsize > 0);

    if (msgsize == 0)
        return NULL;

    msg = (BaseMsg*) malloc(msgsize);
    memset(msg, 0, msgsize);
    msg->msgno = msgno;
    return msg;
}
void free_msg(void *msg) {
    free(msg);
}

// Copy len bytes from src to dst, and null-terminate dst.
// dst should be at least len+1 bytes to accomodate null terminator.
static void assign_sz(char *dst, char *src, size_t len) {
    memcpy(dst, src, len);
    dst[len] = 0;
}

// loginmsg => bs
void pack_loginmsg_to_bs(LoginMsg *loginmsg, char *bs) {
}

// bs => loginmsg
void unpack_bs_to_loginmsg(char *bs, LoginMsg *loginmsg) {
}


