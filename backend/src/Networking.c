#include "Networking.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_IN_LEN 1024
#define MAX_OUT_LEN 1024

// Socket functions //

// Listener socket (server)
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
        if ((listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
            -1) {
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

// Connected socket (client)
int connected_socket(char *hostname, char *port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Get address information
    if ((status = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    // Loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
            -1) {
            perror("socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Failed to connect\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(servinfo);

    return sockfd;
}

// SSL functions //

// Accept the connection
void secure_accept(SSL *ssl) {
    int status = SSL_accept(ssl);
    if (status <= 0) {
        ERR_print_errors_fp(stderr);
        exit(1);
    }
}

// Connect to server
void secure_connect(SSL *ssl) {
    int status = SSL_connect(ssl);
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
    int bytes_read = SSL_read(ssl, buff, buff_len);
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

// HTTPS functions //

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

    while ((bytes_recv = secure_read(ssl, req_buff + total_bytes_recv,
                                     MAX_REQUEST_LEN - total_bytes_recv - 1)) >
           0) {
        total_bytes_recv += bytes_recv;

        // Break if received complete message or Filled the req_buff
        if (req_buff[total_bytes_recv - 1] == '\n' ||
            total_bytes_recv == MAX_REQUEST_LEN - 1)
            break;
    }

    if (total_bytes_recv <= 0) {
        fprintf(stderr, "SSL_read failed with error %d\n",
                SSL_get_error(ssl, bytes_recv));
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    req_buff[total_bytes_recv] = '\0';

    // Parse the request
    parse_http_request(req_buff, request->method, request->file,
                       request->query);

    return request;
}

// Fill to_fill with the value of the param in the query
void fill_query_param(char *query, char param, char *to_fill) {
    // query: ?f=...&t=...&s=...&n=...
    while (*query) {
        if (*query == param && *(query + 1) == '=') {
            query += 2;  // Skip param and '='
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

// Redirect to another route
void redirect(SSL *ssl, char *redirect_to) {
    char response[256];
    int response_len = snprintf(response, sizeof(response),
                                "HTTP/1.1 302 Found\r\n"
                                "Location: %s\r\n"
                                "Content-Length: 0\r\n"
                                "Connection: close\r\n"
                                "\r\n",
                                redirect_to);
    secure_send(ssl, response, response_len);
}

// SMTPS functions //

// Create email
void create_email(char *out, int out_len, const char *from, const char *to,
                  const char *subject, const char *body) {
    snprintf(out, 2048,
             "Content-Type: multipart/mixed; "
             "boundary=\"===============2463790331611798368==\"\r\n"
             "MIME-Version: 1.0\r\n"
             "From: %s\r\n"
             "To: %s\r\n"
             "Subject: %s\r\n\r\n"
             "--===============2463790331611798368==\r\n"
             "Content-Type: text/html; charset=\"us-ascii\"\r\n"
             "MIME-Version: 1.0\r\n"
             "Content-Transfer-Encoding: 7bit\r\n\r\n"
             "%s"
             "\r\n.\r\n",
             from, to, subject, body);
}

// Send email
void *secure_send_email(void *args) {
    char in[MAX_IN_LEN], out[MAX_OUT_LEN];
    email_args *emailargs = (email_args *)args;
    SSL *ssl = emailargs->ssl;
    const char *mydomain = emailargs->mydomain, *from = emailargs->from,
               *auth_token = emailargs->auth_token, *to = emailargs->to,
               *subject = emailargs->subject, *body = emailargs->body;

    // Start SMTPS session
    snprintf(out, MAX_OUT_LEN, "ehlo %s\r\n", mydomain);
    secure_send(ssl, out, strlen(out));
    sleep(1);  // Server sends multiple responses
               // Wait for the server to send all the responses
    secure_read(ssl, in, MAX_IN_LEN);
    snprintf(out, MAX_OUT_LEN, "STARTTLS\r\n");
    secure_send(ssl, out, strlen(out));
    sleep(1);
    secure_read(ssl, in, MAX_IN_LEN);

    // Logging in
    snprintf(out, MAX_OUT_LEN, "ehlo %s\r\n", mydomain);
    secure_send(ssl, out, strlen(out));
    sleep(1);
    secure_read(ssl, in, MAX_IN_LEN);
    snprintf(out, MAX_OUT_LEN, "AUTH PLAIN %s\r\n", auth_token);
    secure_send(ssl, out, strlen(out));
    sleep(1);
    secure_read(ssl, in, MAX_IN_LEN);

    // Send email
    snprintf(out, MAX_OUT_LEN, "mail FROM:<%s>\r\n", from);
    secure_send(ssl, out, strlen(out));
    sleep(1);
    secure_read(ssl, in, MAX_IN_LEN);
    snprintf(out, MAX_OUT_LEN, "rcpt TO:<%s>\r\n", to);
    secure_send(ssl, out, strlen(out));
    sleep(1);
    secure_read(ssl, in, MAX_IN_LEN);
    secure_send(ssl, "data\r\n", 6);
    sleep(1);
    secure_read(ssl, in, MAX_IN_LEN);

    // Send email
    create_email(out, MAX_OUT_LEN, from, to, subject, body);
    secure_send(ssl, out, strlen(out));
    sleep(1);
    secure_read(ssl, in, MAX_IN_LEN);

    // End session
    secure_send(ssl, "QUIT\r\n", 6);

    return NULL;
}
