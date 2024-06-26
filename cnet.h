#ifndef CNET_H
#define CNET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "clib.h"

// Function return values:
// Z_OPEN (fd still open)
// Z_EOF (end of file)
// Z_ERR (errno set with error detail)
// Z_BLOCK (fd blocked, no data)
#define Z_OPEN 1
#define Z_EOF 0
#define Z_ERR -1
#define Z_BLOCK -2

int recv_buf_flush(int fd, buf_t *buf);
int send_buf_flush(int fd, buf_t *buf);

int recv_buf(int fd, buf_t *buf, size_t nbytes, size_t *num_bytes_received);
int recv_line(int fd, buf_t *buf, size_t nbytes, str_t *ret_line, int *ret_line_complete);

int open_listen_sock(char *host, char *port, int backlog, struct sockaddr *psa);
int open_connect_sock(char *host, char *port, struct sockaddr *psa);
void set_sock_timeout(int sock, int nsecs, int ms);
void set_sock_nonblocking(int sock);
unsigned short get_sockaddr_port(struct sockaddr *sa);
void get_ipaddr_string(struct sockaddr *sa, str_t *ipaddr);


#endif

