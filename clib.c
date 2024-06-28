#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include "clib.h"

void quit(const char *s) {
    if (s)
        printf("%s\n", s);
    exit(0);
}
void print_error(const char *s) {
    if (s)
        fprintf(stderr, "%s: %s\n", s, strerror(errno));
    else
        fprintf(stderr, "%s\n", strerror(errno));
}
void panic(const char *s) {
    if (s)
        fprintf(stderr, "%s\n", s);
    abort();
}
void panic_err(const char *s) {
    if (s)
        fprintf(stderr, "%s: %s\n", s, strerror(errno));
    abort();
}

buf_t *buf_new(size_t cap) {
    if (cap == 0) {
        cap = SIZE_TINY;
    }

    buf_t *buf = malloc(sizeof(buf_t));
    buf->cur = 0;
    buf->len = 0;
    buf->cap = cap;
    buf->p = malloc(cap);
    return buf;
}
void buf_free(buf_t *buf) {
    assert(buf->p != NULL);
    free(buf->p);
    buf->p = NULL;
    free(buf);
}
void buf_resize(buf_t *buf, size_t cap) {
    buf->cap = cap;
    if (buf->len > buf->cap) {
        buf->len = buf->cap;
    }
    if (buf->cur >= buf->len) {
        buf->cur = buf->len-1;
    }
    buf->p = realloc(buf->p, cap);
}
void buf_clear(buf_t *buf) {
    memset(buf->p, 0, buf->len);
    buf->len = 0;
    buf->cur = 0;
}
void buf_append(buf_t *buf, char *bs, size_t len) {
    // If not enough capacity to append bytes, expand the buffer.
    if (len > buf->cap - buf->len) {
        char *bs = realloc(buf->p, buf->cap + len);
        if (bs == NULL) {
            panic("buf_append() not enough memory");
        }
        buf->p = bs;
        buf->cap += len;
    }
    memcpy(buf->p + buf->len, bs, len);
    buf->len += len;
}

str_t *str_new(size_t cap) {
    str_t *str;

    if (cap == 0)
        cap = SIZE_SMALL;

    str = (str_t*) malloc(sizeof(str_t));
    str->s = (char*) malloc(cap);
    memset(str->s, 0, cap);
    str->len = 0;
    str->cap = cap;

    return str;
}
void str_free(str_t *str) {
    memset(str->s, 0, str->cap);
    free(str->s);
    free(str);
}
str_t *str_new_assign(const char *s) {
    str_t *str = str_new(strlen(s)+1);
    str_assign(str, s);
    return str;
}
void str_assign(str_t *str, const char *s) {
    size_t s_len = strlen(s);
    if (s_len+1 > str->cap) {
        str->cap *= 2;
        str->s = (char*) realloc(str->s, str->cap);
    }

    strncpy(str->s, s, s_len);
    str->s[s_len] = 0;
    str->len = s_len;
}
void str_sprintf(str_t *str, const char *fmt, ...) {
    char buf[4096];
    va_list args;
    int z;

    va_start(args, fmt);
    z = vsnprintf(buf, sizeof(buf)-1, fmt, args);
    buf[sizeof(buf)-1] = 0;
    va_end(args);

    if (z < 0)
        return;
    str_assign(str, buf);
}
void str_append(str_t *str, const char *s) {
    size_t s_len = strlen(s);
    if (str->len + s_len + 1 > str->cap) {
        str->cap = str->len + s_len + 1;
        str->s = realloc(str->s, str->cap);
    }

    strncpy(str->s + str->len, s, s_len);
    str->len = str->len + s_len;
}

array_t *array_new(size_t cap, voidpfunc_t clear_item_func) {
    if (cap == 0)
        cap = 8;
    array_t *a = (array_t*) malloc(sizeof(array_t));
    a->items = (void**) malloc(sizeof(a->items[0]) * cap);
    a->len = 0;
    a->cap = cap;
    a->clear_item_func = clear_item_func;
    return a;
}
void array_free(array_t *a) {
    array_clear(a);
    free(a->items);
    free(a);
}
static inline void clear_item(array_t *a, int i) {
    if (a->clear_item_func != NULL)
        (*a->clear_item_func)(a->items[i]);
}
void array_clear(array_t *a) {
    for (int i=0; i < a->len; i++)
        clear_item(a, i);
    memset(a->items, 0, a->cap * sizeof(void*));
    a->len = 0;
}
void array_resize(array_t *a, size_t newcap) {
    assert(newcap > a->cap);
    void **p = (void**) realloc(a->items, newcap * sizeof(void*)); 
    if (p == NULL)
        panic("array_resize() out of memory");
    a->items = p;
    a->cap = newcap;
    if (a->len > a->cap)
        a->len = a->cap;
}
void array_add(array_t *a, void *p) {
    if (a->len >= a->cap)
        array_resize(a, a->cap * 2);
    a->items[a->len] = p;
    a->len++;
}
void array_del(array_t *a, uint idx) {
    for (size_t i=idx; i < a->len-1; i++) {
        clear_item(a, i);
        a->items[i] = a->items[i+1];
    }
    a->len--;
}

