#include "Networking.h" 
#include "TLS.h"
#include "DB.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#define TIMEZONE "UTC"
#define PORT "443"
#define BACKLOG 100

#define CERTIFICATE_PATH "/home/sulabhkatila/cerver/assets/certificate.pem"
#define KEY_PATH "/home/sulabhkatila/cerver/assets/privkey.pem"

#define DATABASE_PATH "/home/sulabhkatila/cerver/server.db"
#define SIGNATURE_RECEIPTS_TABLE "signature_receipts"

#define MAX_FILE_BUFF_LEN 2048
#define MAX_DATE_LEN 20
#define MAX_TIME_LEN 20
#define MAX_EMAIL_LEN 50
#define MAX_SUBJECT_LEN 100
#define MAX_N_CODE_LEN 10

void handle_zombie_process();
void setup_timezone(char *timezone);
void fill_date_and_time(char *datestr, char *timestr);

int main() {
    struct sigaction *sa;
    handle_zombie_process(sa);

    // Set timezone to, later, log to database
    setup_timezone(TIMEZONE);

    // Start the server
    int listenerfd = listener_socket(PORT, BACKLOG), newfd;
    struct sockaddr_storage cl_addr;
    char cl_addr4[INET_ADDRSTRLEN];
    socklen_t cl_sin_size;

    SSL_CTX *ctx = ssl_context(CERTIFICATE_PATH, KEY_PATH);
    SSL *ssl;

    printf("\nserver: listening at port %s\n", PORT);
    fflush(stdout);
    // Keep accepting connections
    while (1) {
        // Client information
        cl_sin_size = sizeof cl_addr;
        newfd = accept(listenerfd, (struct sockaddr *)&cl_addr, &cl_sin_size);
        if (newfd == -1) {
            perror("accpet");
            continue;
        }

        inet_ntop(cl_addr.ss_family, &((struct sockaddr_in *)&cl_addr)->sin_addr, cl_addr4, sizeof cl_addr4);
        printf("server: got connection from %s\n", cl_addr4);

        // Get a child process handle the connection
        // Have the parent process accepting new connections
        if (!fork()) {
            close(listenerfd);

            ssl = SSL_new(ctx);
            SSL_set_fd(ssl, newfd);
            secure_accept(ssl);

            Http_request *request = get_request(ssl); 

            // Send response
            pthread_t log_t;
            int is_logging = 0;
            if (strcmp(request->file, "/") == 0) {
                char *header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
                char *body = "<h1>Hello, I am <a href=\"https://sulabhkatila.github.io/\">Sulabh Katila</a>!</h1>";

                secure_send(ssl, header, strlen(header));
                secure_send(ssl, body, strlen(body));
            } else if (strcmp(request->file, "/signature.gif") == 0) {
                // Log the receipt of request to signature_receipts
                char datestr[MAX_DATE_LEN], timestr[MAX_TIME_LEN];
                fill_date_and_time(datestr, timestr);

                char from[MAX_EMAIL_LEN], to[MAX_EMAIL_LEN], subject[MAX_SUBJECT_LEN], n_code[MAX_N_CODE_LEN];
                fill_query_param(request->query, 'f', from);
                fill_query_param(request->query, 't', to);
                fill_query_param(request->query, 's', subject);
                fill_query_param(request->query, 'n', n_code);

                db_args sr_args = {
                    DATABASE_PATH,
                    SIGNATURE_RECEIPTS_TABLE,
                    cl_addr4,
                    from,
                    to,
                    subject,
                    n_code,
                    datestr,
                    timestr,
                };

                pthread_create(&log_t, NULL, log_to_db, (void *)&sr_args);
                is_logging = 1;

                char *header = "HTTP/1.1 200 OK\r\nContent-Type: image/gif\r\n\r\n";
                secure_send(ssl, header, strlen(header));
                
                FILE *f = fopen("/home/sulabhkatila/cerver/files/signature.gif", "r");
                if (f == NULL) {
                    perror("fopen");
                    exit(1);
                }
                
                char f_buff[MAX_FILE_BUFF_LEN];
                int bytes_read;
                while ((bytes_read = fread(f_buff, 1, sizeof(f_buff), f)) > 0) {
                    secure_send(ssl, f_buff, bytes_read);
                }

                fclose(f);
            } else {
                // 404 Not Found
                char *header = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n";
                char *body = "<h1>404 Not Found</h1>";

                secure_send(ssl, header, strlen(header));
                secure_send(ssl, body, strlen(body));
            }
            free(request);

            secure_close(ssl);
            close(newfd);
            printf("server: connection closed with %s\n", cl_addr4);
            if (is_logging) pthread_join(log_t, NULL);
            exit(0);
        }

        close(newfd);
    }

    return 0;
}


// Signal handing for Zombie processes
void handle_sigchld(int sig) {
    // waitpid() might change errno
    // so we save it and restore it later
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
    errno = saved_errno;
}

void handle_zombie_process(struct sigaction *sa) {
    sa->sa_handler = handle_sigchld;
    sigemptyset(&(sa->sa_mask));
    sa->sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
}


// Date and Time handling
void setup_timezone(char *timezone) {
    setenv("TZ", timezone, 1);
    tzset();
}

void fill_date_and_time(char *datestr, char *timestr) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    strftime(datestr, MAX_DATE_LEN - 1, "%Y-%m-%d", tm);
    strftime(timestr, MAX_TIME_LEN - 1, "%H:%M:%S", tm);
    datestr[MAX_DATE_LEN - 1] = '\0';
    timestr[MAX_TIME_LEN - 1] = '\0';
}

