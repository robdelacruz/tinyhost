#ifndef MSG_H
#define MSG_H

// Message numbers
#define TEXTMSG_NO 100

// Maximum size of message body
#define MSG_MAX_BODYLEN 10000

// Message header binary format:
// [4 bytes] protocol signature (Hardcoded to "TINY")
// [5 bytes] protocol version number (Ex. "0.9", "1.1", "22.99")
// [16 bytes] agent name (Ex. "tinyclient", "tinyserver")
// [2 bytes] 16-bit int specifying message number (Ex. 100, 101)
// [2 bytes] 16-bit int specifying the number of bytes in message body
//
// Ascii fields are always right padded with nulls to fill space.
#define MSG_SIG         "TINY"
#define MSG_SIG_LEN     4
#define MSG_VER         "0.9"
#define MSG_VER_LEN     5
#define MSG_AGENT       "tinyserver"
#define MSG_AGENT_LEN   16
#define MSG_MSGNO_LEN   2
#define MSG_BODYLEN_LEN 2
#define MSG_HEADER_LEN  (MSG_SIG_LEN + MSG_VER_LEN + MSG_AGENT_LEN + MSG_MSGNO_LEN +  MSG_BODYLEN_LEN)

#define MSG_OFFSET_SIG(p)     ((p) + 0)
#define MSG_OFFSET_VER(p)     ((p) + MSG_SIG_LEN)
#define MSG_OFFSET_AGENT(p)   ((p) + MSG_SIG_LEN + MSG_VER_LEN)
#define MSG_OFFSET_MSGNO(p)   ((short*)((p) + MSG_SIG_LEN + MSG_VER_LEN + MSG_AGENT_LEN))
#define MSG_OFFSET_BODYLEN(p) ((short*)((p) + MSG_SIG_LEN + MSG_VER_LEN + MSG_AGENT_LEN + MSG_MSGNO_LEN))
#define MSG_OFFSET_BODY(p)    ((p) + MSG_HEADER_LEN)

// TextMsg body binary format:
#define TEXTMSG_ALIAS_LEN     32
#define TEXTMSG_TEXT_LEN      255
#define TEXTMSG_LEN           (TEXTMSG_ALIAS_LEN + TEXTMSG_TEXT_LEN)

#define TEXTMSG_OFFSET_ALIAS(p) (MSG_OFFSET_BODY(p) + 0)
#define TEXTMSG_OFFSET_TEXT(p)  (MSG_OFFSET_BODY(p) + TEXTMSG_ALIAS_LEN)

typedef struct {
    float ver;
    char agent[MSG_AGENT_LEN+1];
    short msgno;
    short bodylen;
} MsgHeader;

#define MSGNO(p) (((BaseMsg *)(p))->msgno)

typedef struct {
    short msgno;
} BaseMsg;

typedef struct {
    short msgno;
    char alias[TEXTMSG_ALIAS_LEN+1];
    char text[TEXTMSG_TEXT_LEN+1];
} TextMsg;

void free_msg(void *msg);
void *unpack_msg_bytes(char *bs);
char *pack_msg(void *msg);

#endif

