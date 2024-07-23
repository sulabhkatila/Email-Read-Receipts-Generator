#ifndef _NETWORKING_H_
#define _NETWORKING_H_

#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

typedef struct email_args {
    SSL *ssl;
    const char *mydomain;
    const char *from;
    const char *auth_token;
    const char *to;
    const char *subject;
    const char *body;
} email_args;

// Socket related functions
int listener_socket(char *port, int backlog);
int connected_socket(char *host, char *port);

// HTTPS functions
Http_request *get_request(SSL *ssl);
void fill_query_param(char *query, char param, char *to_fill);
void redirect(SSL *ssl, char *redirect_to);

// SMPTS functions
void *secure_send_email(void *args);

// SSL functions for HTTPS and SMTPS
void secure_accept(SSL *ssl);
void secure_connect(SSL *ssl);
void secure_send(SSL *ssl, const char *data, size_t data_len);
int secure_read(SSL *ssl, char *read_buff, size_t read_buff_len);
void secure_close(SSL *ssl);

#endif
