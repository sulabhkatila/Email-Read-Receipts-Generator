#include "Networking.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int listener_socket(char *port, int backlog) {
    int listener;
    struct addrinfo hints, *res, *p;

    // Get the address info
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status;
    if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    // Bind to the first available address
    for (p = res; p != NULL; p = p->ai_next) {
        if ((listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {   
            // AF_INET and PF_INET are basically the same thing
            perror("server: socket");
            continue;
        }

        if (bind(listener, p->ai_addr, p->ai_addrlen) == -1) {
            close(listener);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(res);

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    if (listen(listener, backlog) == -1) {
        perror("listen");
        return 3;
    }

    return listener;
}

// Accept the connection
void secure_accept(SSL *ssl) {
    int status = SSL_accept(ssl);
    if (status <= 0) {
        ERR_print_errors_fp(stderr);
        exit(1);
    }
}

// Send complete encrypted responses
void secure_send(SSL *ssl, const char *data, size_t data_len) {
    size_t bytes_sent = 0;
    int result;

    while (bytes_sent < data_len) {
        result = SSL_write(ssl, data + bytes_sent, data_len - bytes_sent);
        if (result <= 0) {
            ERR_print_errors_fp(stderr);
            exit(1);
        }
        bytes_sent += result;
    }
}

// Read encrypted response
int secure_read(SSL *ssl, char *buff, size_t buff_len) {
    int bytes_read =  SSL_read(ssl, buff, buff_len);
    if (bytes_read < 0) {
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    return bytes_read;
}

// Close the connection
void secure_close(SSL *ssl) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
}

// Get the request from the client
void parse_http_request(char *req_buff, char *method, char *file, char *query) {
    // GET /file?query HTTP/1.1
    // ...
    char *temp_file;
    char *temp_query;

    // Parse method
    char *temp_method = strtok(req_buff, " ");
    if (temp_method) {
        strcpy(method, temp_method);
    } else {
        method[0] = '\0';
    }

    // Parse file and query
    temp_file = strtok(NULL, " ");
    if (temp_file) {
        temp_query = strchr(temp_file, '?');
        if (temp_query) {
            *temp_query = '\0';  // terminate the file string at the query
            strcpy(query, temp_query + 1);  // copy the query part
        } else {
            query[0] = '\0';  // empty query
        }
        strcpy(file, temp_file);
    } else {
        file[0] = '\0';
        query[0] = '\0';
    }
}

Http_request *get_request(SSL *ssl) {
    Http_request *request = (Http_request *)malloc(sizeof(Http_request));

    // Read the request
    char req_buff[MAX_REQUEST_LEN];
    int bytes_recv = 0, total_bytes_recv = 0;

    while ((bytes_recv = secure_read(ssl, req_buff + total_bytes_recv, MAX_REQUEST_LEN - total_bytes_recv - 1)) > 0)
    {
        total_bytes_recv += bytes_recv;

        // Break if received complete message or Filled the req_buff
        if (req_buff[total_bytes_recv - 1] == '\n' || total_bytes_recv == MAX_REQUEST_LEN - 1)
            break;
    }

    if (total_bytes_recv <= 0)
    {
        fprintf(stderr, "SSL_read failed with error %d\n", SSL_get_error(ssl, bytes_recv));
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    req_buff[total_bytes_recv] = '\0';

    // Parse the request
    parse_http_request(req_buff, request->method, request->file, request->query);

    return request;
}

// Fill to_fill with the value of the param in the query
void fill_query_param(char *query, char param, char *to_fill) {
    // query: ?f=...&t=...&s=...&n=...    
    while (*query) {
        if (*query == param && *(query + 1) == '=') {
            query += 2; // Skip param and '='
            while (*query && *query != '&') {
                *(to_fill++) = *query++;
            }
            *to_fill = '\0';
            return;
        }
        query++;
    }
    // If param is not found, fill to_fill with an empty string
    *to_fill = '\0';
}

