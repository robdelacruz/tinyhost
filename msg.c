#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <arpa/inet.h>
#include "msg.h"

short _msgtbl[] = {
    TEXTMSG_NO, TEXTMSG_LEN,
    0
};

static short lookup_bodylen(short msgno) {
    short *p = _msgtbl;
    while (*p != 0) {
        if (*p == msgno)
            return *(p+1);
        p += 2;
    }
    return -1;
}

static int msgno_is_valid(short msgno) {
    if (lookup_bodylen(msgno) >= 0)
        return 1;
    return 0;
}

// Copy len bytes from src to dst, and null-terminate dst.
// dst should be at least len+1 bytes to accomodate null terminator.
static void assign_sz(char *dst, char *src, size_t len) {
    memcpy(dst, src, len);
    dst[len] = 0;
}

// Copies up to len sz chars to dst, padding dst with zeroes to fill len.
static void copystr_padzero(char *dst, char *sz, size_t len) {
    strncpy(dst, sz, len);
}

void free_msg(void *msg) {
    free(msg);
}

void unpack_msg_header(char *bs, MsgHeader *mh) {
    char sver[MSG_VER_LEN+1];

    assign_sz(sver, MSG_OFFSET_VER(bs), MSG_VER_LEN);
    mh->ver = atof(sver);
    assign_sz(mh->agent, MSG_OFFSET_AGENT(bs), MSG_AGENT_LEN);
    mh->msgno = ntohs(*MSG_OFFSET_MSGNO(bs));
    mh->bodylen = ntohs(*MSG_OFFSET_BODYLEN(bs));
}

void *unpack_msg_bytes(char *bs) {
    short msgno = ntohs(*MSG_OFFSET_MSGNO(bs));
    short bodylen = ntohs(*MSG_OFFSET_BODYLEN(bs));

    printf("unpack_msg_bytes() msgno: %d, bodylen: %d\n", msgno, bodylen);

    if (lookup_bodylen(msgno) != bodylen) {
        printf("unpack_msg_bytes(): invalid message (msgno: %d, bodylen: %d)\n", msgno, bodylen);
        return NULL;
    }

    if (msgno == TEXTMSG_NO) {
        TextMsg *m = malloc(sizeof(TextMsg));
        m->msgno = msgno;
        assign_sz(m->alias, TEXTMSG_OFFSET_ALIAS(bs), TEXTMSG_ALIAS_LEN);
        assign_sz(m->text, TEXTMSG_OFFSET_TEXT(bs), TEXTMSG_TEXT_LEN);
        return m;
    }

    printf("unpack_msg_bytes(): msgno %d not supported.\n", msgno);
    return NULL;
}

char *pack_msg(void *msg) {
    short msgno = MSGNO(msg);
    short bodylen = lookup_bodylen(msgno);
    if (bodylen < 0) {
        printf("pack_msg(): invalid message (msgno: %d)\n", msgno);
        return NULL;
    }
    printf("pack_msg() msgno: %d, bodylen: %d\n", msgno, bodylen);

    char *bs = malloc(MSG_HEADER_LEN + bodylen);
    memset(bs, 0, MSG_HEADER_LEN + bodylen);

    copystr_padzero(MSG_OFFSET_SIG(bs), MSG_SIG, MSG_SIG_LEN);
    copystr_padzero(MSG_OFFSET_VER(bs), MSG_VER, MSG_VER_LEN);
    copystr_padzero(MSG_OFFSET_AGENT(bs), MSG_AGENT, MSG_AGENT_LEN);
    *MSG_OFFSET_MSGNO(bs) = htons(msgno);
    *MSG_OFFSET_BODYLEN(bs) = htons(bodylen);

    if (msgno == TEXTMSG_NO) {
        TextMsg *textmsg = msg;
        copystr_padzero(TEXTMSG_OFFSET_ALIAS(bs), textmsg->alias, TEXTMSG_ALIAS_LEN);
        copystr_padzero(TEXTMSG_OFFSET_TEXT(bs), textmsg->text, TEXTMSG_TEXT_LEN);
        return bs;
    }

    free(bs);
    printf("pack_msg(): msgno %d not supported.\n", msgno);
    return NULL;
}

