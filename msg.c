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

void free_msg(void *msg) {
    free(msg);
}

