#include "TLS.h"

#include <stdio.h>

void configure_context(SSL_CTX *ctx, const char *certificate_path,
                       const char *key_path) {
    if (SSL_CTX_use_certificate_file(ctx, certificate_path, SSL_FILETYPE_PEM) <=
        0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

SSL_CTX *ssl_context(const char *certificate_path, const char *key_path,
                     char type) {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = (type == 's') ? TLS_server_method() : TLS_client_method();
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    configure_context(ctx, certificate_path, key_path);

    return ctx;
}
