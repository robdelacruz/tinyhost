#ifndef MSG_H
#define MSG_H

#define MSGHEAD_SIZE 5

// Message numbers
#define LOGIN     100
#define LOGOUT    101
#define SEND_TEXT 102

// Message body lengths
#define LOGIN_LEN     35
#define LOGOUT_LEN    0
#define SEND_TEXT_LEN 90

typedef struct {
    char msgno;
} BaseMsg;

// [0]  username (20 bytes)
// [20] password (25 bytes)
// len: 35 bytes
typedef struct {
    char msgno;
    char username[20+1];
    char password[25+1];
} LoginMsg;

typedef struct {
    char msgno;
} LogoutMsg;

// [0]  to_username (20 bytes)
// [20] from_username (20 bytes)
// [41] text (50 bytes)
// len: 90 bytes
typedef struct {
    char msgno;
    char to_username[20+1];
    char from_username[20+1];
    char text[50+1];
} SendTextMsg;

int read_msghead(char *msghead, short *ver, char *msgno);
int isvalid_msgno(char msgno, short ver);
int msgbody_bytes_size(char msgno, short ver);
void *create_msg(char msgno, short ver);
void free_msg(void *msg);

void pack_loginmsg_to_bs(LoginMsg *loginmsg, char *bs);
void unpack_bs_to_loginmsg(char *bs, LoginMsg *loginmsg);

#endif

