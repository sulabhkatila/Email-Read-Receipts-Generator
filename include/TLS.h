#ifndef _TLS_H_
#define _TLS_H_

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <arpa/inet.h>

SSL_CTX *ssl_context(const char *certificate_path, const char *key_path);

void close_ssl(SSL *ssl);

#endif

