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

#define PORT "4000"
#define BACKLOG 10
#define HOSTBUFFER_SIZE 50
#define MAX_REQ_SIZE 2048
#define FILE_BUFF 1024

void fill_my_ip(char *buffer);
void handle_sigchld(int sig);
void handle_request(int fd);
void send_res(int fd, char *path);

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

    int serverfd, newfd;
    struct addrinfo hints, *res, *r;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    char addr4[INET_ADDRSTRLEN]; // IPv4

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

    for (r = res; r != NULL; r = r->ai_next)
    {
        if ((serverfd = socket(r->ai_family, r->ai_socktype, r->ai_protocol)) == -1)
        { // AF_INET and PF_INET are basically the same thing
            perror("server: socket");
            continue;
        }

        if (bind(serverfd, r->ai_addr, r->ai_addrlen) == -1)
        {
            close(serverfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(res);

    if (r == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    if (listen(serverfd, BACKLOG) == -1)
    {
        perror("listen");
        return 3;
    }

    // Print the IP address and Port Numner
    char my_ip[INET_ADDRSTRLEN];
    fill_my_ip(my_ip);
    fprintf(stdout, "server: waiting for connections at %s IP address %s portnumber\n", my_ip, PORT);

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

        if (!fork())
        {
            close(serverfd);

            handle_request(newfd);
            close(newfd);
            exit(0);
        }
        close(newfd);
    }

    return 0;
}

void fill_my_ip(char *buffer)
{
    struct hostent *host_entry;
    char hostbuffer[HOSTBUFFER_SIZE];
    int hostname;

    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    if (hostname == -1)
    {
        perror("gethostname");
        exit(1);
    }

    host_entry = gethostbyname(hostbuffer);
    if (host_entry == NULL)
    {
        perror("gethostbyname");
        exit(1);
    }

    strcpy(buffer, inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0])));
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

void handle_request(int fd)
{
    char req_buff[MAX_REQ_SIZE];
    int bytes_recv;

    if ((bytes_recv = recv(fd, req_buff, MAX_REQ_SIZE - 1, 0)) == -1)
    {
        perror("recv");
        exit(1);
    }
    req_buff[bytes_recv] = '\0';

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

    // Send reponse
    send_res(fd, path);
}

void send_res(int fd, char *path)
{
    if (strcmp(path, "/") == 0)
    {
        char *header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
        char *body = "<h1>Hello, I am <a href=\"https://sulabhkatila.github.io/\">Sulabh Katila</a>!</h1>";
        send(fd, header, strlen(header), 0);
        send(fd, body, strlen(body), 0);
    }
    else if (strcmp(path, "/signature.gif") == 0)
    {
        char *header = "HTTP/1.1 200 OK\r\nContent-Type: image/gif\r\n\r\n";
        send(fd, header, strlen(header), 0);

        FILE *file = fopen("../assets/signature.gif", "r");
        if (file == NULL)
        {
            perror("fopen");
            exit(1);
        }

        char f_buff[FILE_BUFF];
        int bytes_read;
        while ((bytes_read = fread(f_buff, 1, sizeof(f_buff), file)) > 0)
        {
            send(fd, f_buff, bytes_read, 0);
        }

        fclose(file);
    }
    else
    {
        // 404 Not Found
        char *header = "HTTP/1.1 404 NOT FOUND\r\nContent-Type: text/html\r\n\r\n";
        char *body = "<h1>404 Not Found</h1>";
        send(fd, header, strlen(header), 0);
        send(fd, body, strlen(body), 0);
    }
}