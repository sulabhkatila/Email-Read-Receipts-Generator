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

#define MYDOMAIN "emailapi.endpoints.isentropic-card-423523-k4.cloud.goog"

#define TIMEZONE "UTC"
#define PORT "443"
#define BACKLOG 100

#define GOOGLE_PROXY "74.125"

#define MAX_LINE_LEN 100
#define MAX_FILE_BUFF_LEN 2048
#define MAX_DATE_LEN 20
#define MAX_TIME_LEN 20
#define MAX_EMAIL_LEN 50
#define MAX_SUBJECT_LEN 100
#define MAX_N_CODE_LEN 10

#define SMTP_SERVER "smtp.gmail.com"
#define SMTP_PORT "465"

#define EMAIL_SUBJECT "Email Read Receipt"
#define EMAIL_BODY_LEN 512


void handle_zombie_process(struct sigaction *sa);
void load_env_variables();
void setup_timezone(char *timezone);
void fill_date_and_time(char *datestr, char *timestr);

int main(int argc, char *argv[]) {
    // Deal with zombie processes
    struct sigaction sa;
    handle_zombie_process(&sa);

    // Get the environment variables
    load_env_variables();
    const char *EMAIL = getenv("EMAIL");
    const char *AUTH_TOKEN = getenv("AUTH_TOKEN");
    const char *CERTIFICATE_PATH = getenv("CERTIFICATE_PATH");
    const char *KEY_PATH = getenv("KEY_PATH");
    const char *DATABASE_PATH = getenv("DATABASE_PATH");
    const char *SIGNATURE_RECEIPTS_TABLE = getenv("SIGNATURE_RECEIPTS_TABLE");
    if (EMAIL == NULL ||
        AUTH_TOKEN == NULL ||
        CERTIFICATE_PATH == NULL ||
        KEY_PATH == NULL ||
        DATABASE_PATH == NULL ||
        SIGNATURE_RECEIPTS_TABLE == NULL
        ) {
        fprintf(stderr, "Environment variables not set\n");
        exit(1);
    }

    // Set timezone to, later, log to database
    setup_timezone(TIMEZONE);

    // Start the server
    int listenerfd = listener_socket(PORT, BACKLOG), newfd;
    struct sockaddr_storage cl_addr;
    char cl_addr4[INET_ADDRSTRLEN];
    socklen_t cl_sin_size;

    SSL_CTX *ctx = ssl_context(CERTIFICATE_PATH, KEY_PATH, 's');  // Server SSL context
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
            pthread_t log_t, email_t;
            int is_logging = 0, is_emailing = 0;
            if (strcmp(request->file, "/") == 0) {
                char *header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
                char *body = "<h1>Hello, I am <a href=\"https://sulabhkatila.github.io/\">Sulabh Katila</a>!</h1>";

                secure_send(ssl, header, strlen(header));
                secure_send(ssl, body, strlen(body));
            } else if (strcmp(request->file, "/signature.gif") == 0) {
                // Redirect if it was not from Google proxy
                if (strncmp(cl_addr4, GOOGLE_PROXY, strlen(GOOGLE_PROXY)) != 0) {
                    char home_url[100];
                    snprintf(home_url, sizeof(home_url), "https://%s/", MYDOMAIN);
                    redirect(ssl, home_url);    
                } else {

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

                    // Start a SMTPS client and send the email
                    int connected = connected_socket(SMTP_SERVER, SMTP_PORT);   // Connect to SMTP server
                    // SSL/TLS connection
                    SSL *smtp_ssl = SSL_new(ssl_context(CERTIFICATE_PATH, KEY_PATH, 0));  // Client SSL context
                    SSL_set_fd(smtp_ssl, connected);
                    secure_connect(smtp_ssl);

                    // Create the message and send the email
                    char body[EMAIL_BODY_LEN];
                    snprintf(   body,
                                EMAIL_BODY_LEN,  
                                "<p>Hello, %s!</p>"
                                "<p>Your signature has been received by %s (Subject: %s) on %s at %s (%s).</br></p>"
                                "<p></p><p></p><p>Thank you!</p>",
                                from, to, subject, datestr, timestr, TIMEZONE
                            );
                    email_args ea = {
                        smtp_ssl,
                        MYDOMAIN,
                        EMAIL,
                        AUTH_TOKEN,
                        from,
                        EMAIL_SUBJECT,
                        body,
                    };
                    pthread_create(&email_t, NULL, secure_send_email, (void *)&ea);
                    is_emailing = 1;

                    // Send the response
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
                    if (is_emailing) {
                        pthread_join(email_t, NULL);
                        secure_close(smtp_ssl);
                        close(connected);
                    }
                }
            } else {
                // 404 Not Found
                char *header = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n";
                char *body = "<h1>404 Not Found</h1>";

                secure_send(ssl, header, strlen(header));
                secure_send(ssl, body, strlen(body));
            }
            free(request);

            // The End
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


// Environment variables
void load_env_variables() {
    FILE *env_file = fopen(".env", "r");
    if (env_file == NULL) {
        return;     // No .env file
                    // Use the environment variables set in the shell
    }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), env_file)) {
        char *name = strtok(line, "=");
        char *value = strtok(NULL, "\n");
        setenv(name, value, 1);
    }

    fclose(env_file);
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
