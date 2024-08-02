#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <arpa/inet.h>
#include "msg.h"

// Copy len bytes from src to dst, and null-terminate dst.
// dst should be at least len+1 bytes to accomodate null terminator.
static void assign_sz(char *dst, char *src, size_t len) {
    memcpy(dst, src, len);
    dst[len] = 0;
}

// Base message bytes:
// [0-3] "TINY"
// [4-7] version number (4 ascii bytes)
// [8] message number (1-255)
int parse_basemsg_bytes(char *bs, char *msgno, int16 *ver) {
    char versionstr[5];

    *msgno = 0;
    *ver = 0;

    // signature starts with TINY followed by 4 digit version number.
    // Ex. TINY0900 (version 0.9)
    //     TINY2500 (version 2.5)
    if (strncmp(bs, "TINY", 4) != 0)
        return 0;
    assign_sz(versionstr, bs+4, 4);

    *msgno = bs[8];
    *ver = atoi(versionstr);
    return isvalid_msgno(*msgno, *ver);
}
int isvalid_msgno(char msgno, int16 ver) {
    if (msgbody_bytes_size(msgno, ver) > 0)
        return 1;
    return 0;
}
int msgbody_bytes_size(char msgno, int16 ver) {
    if (ver != 1)
        return 0;
    if (msgno == MSGNO_ENTER)
        return ENTER_LEN;
    else if (msgno == MSGNO_BYE)
        return BYE_LEN;
    else if (msgno == MSGNO_TEXT)
        return TEXT_LEN;
    else
        return 0;
}

static size_t msg_structsize(char msgno, int16 ver) {
    if (ver != 1)
        return 0;
    if (msgno == MSGNO_ENTER)
        return sizeof(EnterMsg);
    else if (msgno == MSGNO_BYE)
        return sizeof(ByeMsg);
    else if (msgno == MSGNO_TEXT)
        return sizeof(TextMsg);
    else
        return 0;
}
void *create_msg(char msgno, int16 ver) {
    BaseMsg *msg;
    size_t msgsize = msg_structsize(msgno, ver);
    assert(msgsize > 0);

    if (msgsize == 0)
        return NULL;

    msg = (BaseMsg*) malloc(msgsize);
    memset(msg, 0, msgsize);
    msg->msgno = msgno;
    msg->ver = ver;
    return msg;
}
void free_msg(void *msg) {
    free(msg);
}

// bytes => message struct as determined by msgno/ver
void unpack_msg_bytes(char *bs, BaseMsg *msg) {
}

// EnterMsg => bytes
void pack_entermsg_struct(EnterMsg *msg, char *bs) {
}

// bytes => EnterMsg
void unpack_entermsg_bytes(char *bs, EnterMsg *msg) {
}


