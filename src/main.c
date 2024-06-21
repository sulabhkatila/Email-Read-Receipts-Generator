#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sqlite3.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define PORT "443"
#define BACKLOG 100
#define HOSTBUFFER_SIZE 50
#define MAX_REQ_SIZE 2048
#define FILE_BUFF 1024
#define DATE_LEN 15
#define TIME_LEN 15

void handle_sigchld(int sig);
int get_listner_socket();
void handle_https_request(SSL *ssl);
void *log_to_signature(void *l);
void fill_date_and_time(char *datestr, char *timestr);
void send_https_res(SSL *ssl, char *path, char *query);
SSL_CTX *create_context();
void configure_context(SSL_CTX *ctx);

typedef struct signature_log_args
{
    char *f;
    char *t;
    char *s;
    char *n;
} signature_log_args;

int main()
{

    // Deal with zombie processes
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    // Set the timezone to UTC
    setenv("TZ", "UTC", 1);
    tzset();

    int serverfd, newfd; // Listen on serverfd
                         // new connection on newfd

    // Client info
    struct sockaddr_storage client_addr;
    char addr4[INET_ADDRSTRLEN]; // IPv4
    socklen_t sin_size;

    // TLS/SSL
    SSL_CTX *ctx;
    SSL *ssl;

    serverfd = get_listner_socket();
    ctx = create_context();
    configure_context(ctx);
    printf("server: waiting for connections...\n");

    // Keep accepting connections
    while (1)
    {
        sin_size = sizeof client_addr;
        newfd = accept(serverfd, (struct sockaddr *)&client_addr, &sin_size);
        if (newfd == -1)
        {
            perror("accept");
            continue;
        }

        inet_ntop(client_addr.ss_family, &((struct sockaddr_in *)&client_addr)->sin_addr, addr4, sizeof addr4); // IPv4
        printf("server: got connection from %s\n", addr4);

        // Create a new process and let child process handle the new connection
        // Parent process will continue to accept new connections
        if (!fork())
        {
            close(serverfd);

            ssl = SSL_new(ctx);
            SSL_set_fd(ssl, newfd);

            if (SSL_accept(ssl) <= 0)
            {
                ERR_print_errors_fp(stderr);
            }
            else
            {
                handle_https_request(ssl);
            }

            SSL_shutdown(ssl);
            SSL_free(ssl);

            close(newfd);
            printf("connection closed");
            exit(0);
        }
        close(newfd);
    }

    return 0;
}

void handle_sigchld(int sig)
{
    // waitpid() might change errno
    // so we save it and restore it later
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
    errno = saved_errno;
}

int get_listner_socket()
{
    int listener;
    struct addrinfo hints, *res, *p;

    // Get the address info
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status;
    if ((status = getaddrinfo(NULL, PORT, &hints, &res)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    // Bind to the first available address
    for (p = res; p != NULL; p = p->ai_next)
    {
        if ((listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        { // AF_INET and PF_INET are basically the same thing
            perror("server: socket");
            continue;
        }

        if (bind(listener, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(listener);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(res);

    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    if (listen(listener, BACKLOG) == -1)
    {
        perror("listen");
        return 3;
    }

    return listener;
}

SSL_CTX *create_context()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = TLS_server_method();
    ctx = SSL_CTX_new(method);
    if (!ctx)
    {
        perror("SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

void configure_context(SSL_CTX *ctx)
{
    if (SSL_CTX_use_certificate_file(ctx, "/home/sulabhkatila/cerver/files/certificate.pem", SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "/home/sulabhkatila/cerver/files/privkey.pem", SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

void handle_https_request(SSL *ssl)
{
    char req_buff[MAX_REQ_SIZE];
    int bytes_recv;
    int total_bytes_recv = 0;

    while ((bytes_recv = SSL_read(ssl, req_buff + total_bytes_recv, MAX_REQ_SIZE - total_bytes_recv - 1)) > 0)
    {
        fflush(stdout);
        total_bytes_recv += bytes_recv;

        // Break if received complete message or Filled the req_buff
        if (req_buff[total_bytes_recv - 1] == '\n' || total_bytes_recv == MAX_REQ_SIZE - 1)
            break;
    }

    if (bytes_recv <= 0)
    {
        fprintf(stderr, "SSL_read failed with error %d\n", SSL_get_error(ssl, bytes_recv));
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    req_buff[total_bytes_recv] = '\0';

    printf("The DATA IS: %s\n", req_buff);
    fflush(stdout);

    // Parse the request
    // GET /path?query HTTP/1.1
    // ...
    char *method = strtok(req_buff, " ");
    char *path = strtok(NULL, " ");
    char *query = strchr(path, '?');

    if (query)
    {
        *query = '\0';
        query++;
    }
    else
    {
        query = "";
    }
    char *protocol = strtok(NULL, "\r\n");

    // Send response
    send_https_res(ssl, path, query);
}

void send_all_res(SSL *ssl, const char *data, size_t data_len)
{
    size_t bytes_sent = 0;
    int result;

    while (bytes_sent < data_len)
    {
        result = SSL_write(ssl, data + bytes_sent, data_len - bytes_sent);
        if (result <= 0)
        {
            ERR_print_errors_fp(stderr);
            exit(1);
        }
        bytes_sent += result;
    }
}

void send_https_res(SSL *ssl, char *path, char *query)
{
    if (strcmp(path, "/") == 0)
    {
        char *header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
        char *body = "<h1>Hello, I am <a href=\"https://sulabhkatila.github.io/\">Sulabh Katila</a>!</h1>";

        send_all_res(ssl, header, strlen(header));
        send_all_res(ssl, body, strlen(body));
    }
    else if (strcmp(path, "/signature.gif") == 0)
    {
        pthread_t log_thread;
        int is_logging = 0;

        // query can be ?f=somevalue&t=somevalue&n=somevalue
        if (query)
        {
            char *f = strtok(query, "&");
            char *t = strtok(NULL, "&");
            char *s = strtok(NULL, "&");
            char *n = strtok(NULL, "&");

            signature_log_args *args = malloc(sizeof(signature_log_args));
            if (!args) {
                perror("malloc");
                exit(1);
            }

            args->f = f;
            args->t = t;
            args->s = s;
            args->n = n;

            // log to signature.log
            pthread_create(&log_thread, NULL, log_to_signature, (void *)args);
            is_logging = 1;
        }

        char *header = "HTTP/1.1 200 OK\r\nContent-Type: image/gif\r\n\r\n";
        send_all_res(ssl, header, strlen(header));

        FILE *file = fopen("assets/signature.gif", "r");
        if (file == NULL)
        {
            perror("fopen");
            exit(1);
        }

        char f_buff[FILE_BUFF];
        int bytes_read;
        while ((bytes_read = fread(f_buff, 1, sizeof(f_buff), file)) > 0)
        {
            send_all_res(ssl, f_buff, bytes_read);
        }

        fclose(file);
        if (is_logging)
            pthread_join(log_thread, NULL);
    }
    else
    {
        // 404 Not Found
        char *header = "HTTP/1.1 404 NOT FOUND\r\nContent-Type: text/html\r\n\r\n";
        char *body = "<h1>404 Not Found</h1>";
        send_all_res(ssl, header, strlen(header));
        send_all_res(ssl, body, strlen(body));
    }
}

void *log_to_signature(void *l)
{
    sqlite3 *db;
    char *err_msg = 0;
   
    int rc = sqlite3_open("server.db", &db);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }
    
    char datestr[DATE_LEN], timestr[TIME_LEN];
    fill_date_and_time(datestr, timestr);
    
    char sql[512];
    
    sprintf(sql, "INSERT INTO signature(ip, from_email, to_email, subject, n_code, date, time) VALUES('%s', '%s', '%s', '%s', '%s', '%s', '%s')", 
            "0", ((signature_log_args *)l)->f, ((signature_log_args *)l)->t, ((signature_log_args *)l)->s, ((signature_log_args *)l)->n, datestr, timestr);
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to insert record: %s\n", err_msg);
        sqlite3_free(err_msg);
    }

    sqlite3_close(db);
    
    free(l);
    
    return NULL;
}


void fill_date_and_time(char *datestr, char *timestr)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    
    strftime(datestr, DATE_LEN - 1, "%Y-%m-%d", tm);
    strftime(timestr, TIME_LEN - 1, "%H:%M:%S", tm);
    datestr[DATE_LEN - 1] = '\0';
    timestr[TIME_LEN - 1] = '\0';
}
