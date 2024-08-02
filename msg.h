#ifndef MSG_H
#define MSG_H

typedef int16_t int16;

// Message numbers
#define MSGNO_ENTER 100
#define MSGNO_BYE   101
#define MSGNO_TEXT  102

// Message body lengths
#define ENTER_LEN  32
#define BYE_LEN    0
#define TEXT_LEN   255

// Base message bytes:
// [0-3] "TINY"
// [4-7] version number (4 ascii bytes)
// [8] message number (1 byte: 1-255)
#define BASEMSG_SIZE (4+4+1)
typedef struct {
    char msgno;
    int16 ver;
} BaseMsg;

typedef struct {
    char msgno;
    int16 ver;
    char alias[32+1];
} EnterMsg;

typedef struct {
    char msgno;
    int16 ver;
} ByeMsg;

typedef struct {
    char msgno;
    int16 ver;
    char text[255+1];
} TextMsg;

int parse_basemsg_bytes(char *bs, char *msgno, int16 *ver);
int isvalid_msgno(char msgno, int16 ver);
int msgbody_bytes_size(char msgno, int16 ver);
void *create_msg(char msgno, int16 ver);
void free_msg(void *msg);

void unpack_msg_bytes(char *bs, BaseMsg *msg);

void pack_entermsg_struct(EnterMsg *msg, char *bs);
void unpack_entermsg_bytes(char *bs, EnterMsg *msg);

#endif

