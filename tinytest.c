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

int main(int argc, char *argv[]) {
    TextMsg tm;

    tm.msgno = TEXTMSG_NO;
    strcpy(tm.alias, "rob");
    strcpy(tm.text, "This is a song that took me a long time to write...");

    printf("Packing textmsg...\n");
    char *msgbs = pack_msg(&tm);
    assert(msgbs != NULL);

    printf("Unpacking textmsg bytes...\n");
    TextMsg *tm2 = unpack_msg_bytes(msgbs);
    assert(tm2 != NULL);

    printf("Unpacked message:\n");
    printf("tm2.msgno: %d\n", tm2->msgno);
    printf("tm2.alias: '%s'\n", tm2->alias);
    printf("tm2.text: '%s'\n", tm2->text);

}


