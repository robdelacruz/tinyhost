#ifndef CLIB_H
#define CLIB_H

#define countof(v) (sizeof(v) / sizeof((v)[0]))
#define memzero(p, v) (memset(p, 0, sizeof(v)))

#define SIZE_MB      1024*1024
#define SIZE_TINY    512
#define SIZE_SMALL   1024
#define SIZE_MEDIUM  32768
#define SIZE_LARGE   (1024*1024)
#define SIZE_HUGE    (1024*1024*1024)

typedef struct {
    char *p;
    size_t cur;
    size_t len;
    size_t cap;
} buf_t;

typedef struct {
    char *s;
    size_t len;
    size_t cap;
} str_t;

typedef void (*voidpfunc_t)(void *);
typedef struct {
    void **items;
    size_t len;
    size_t cap;
    voidpfunc_t clear_item_func;
} array_t;

void quit(const char *s);
void print_error(const char *s);
void panic(const char *s);
void panic_err(const char *s);

buf_t *buf_new(size_t cap);
void buf_free(buf_t *buf);
void buf_resize(buf_t *buf, size_t cap);
void buf_clear(buf_t *buf);
void buf_append(buf_t *buf, char *bs, size_t len);

str_t *str_new(size_t cap);
void str_free(str_t *str);
str_t *str_new_assign(const char *s);
void str_assign(str_t *str, const char *s);
void str_sprintf(str_t *str, const char *fmt, ...);
void str_append(str_t *str, const char *s);

array_t *array_new(size_t cap, voidpfunc_t clear_item_func);
void array_free(array_t *a);
void array_clear(array_t *a);
void array_resize(array_t *a, size_t newcap);
void array_add(array_t *a, void *p);
void array_del(array_t *a, uint idx);

#endif

