#ifndef _NETWORKING_H_
#define _NETWORKING_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <unistd.h> 
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MAX_METHOD_LEN 7
#define MAX_FILEPATH_LEN 15
#define MAX_QUERY_LEN 50
#define MAX_VERSION_LEN 5
#define MAX_REQUEST_LEN 1024

typedef struct Http_request {
    char method[MAX_METHOD_LEN];
    char version[MAX_VERSION_LEN];
    char file[MAX_FILEPATH_LEN];
    char query[MAX_QUERY_LEN];
} Http_request;

int listener_socket(char *port, int backlog);
void secure_accept(SSL *ssl);
void secure_send(SSL *ssl, const char *data, size_t data_len);
int secure_read(SSL *ssl, char *read_buff, size_t read_buff_len);
void secure_close(SSL *ssl);
Http_request *get_request(SSL *ssl);
void fill_query_param(char *query, char param, char *to_fill);

#endif

