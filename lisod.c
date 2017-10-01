/*
 * Echo-based server.
 * Modified from Beej's guide and tutorial on
 * http://forum.codecall.net/topic/64205-concurrent-tcp-server-using-select-api-in-linux-c/
 */

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <time.h>
#include "log.h"
#include "parse.h"
#include "process_request.h"

#define ECHO_PORT 9999
#define BUF_SIZE 4096

int close_socket(int sock) {
    log_write("closing sock %d\n", sock);
    if (close(sock)) {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    fd_set master, read_fds; // master and temp list for select()
    int sock, client_sock; //listening socket descriptor and newly accepted socket descriptor
    int i, j, k;
    int client[FD_SETSIZE], nready;
    int maxfd, max_idx;
    int http_port;
    char *www_path, *log_file;
    ssize_t readret; /*ssize_t = the sizes of blocks that can be read or written in a single operation*/
    socklen_t cli_size; /*socklen_t = an integral type of at least 32 bits*/
    struct sockaddr_in addr, cli_addr; /*struct = a user defined data type that allows to combine data items of different kind*/
    char buf[BUF_SIZE];
    int yes = 1;
    struct tm tm;
    struct stat sbuf;
    time_t now;
    char filetype[100], tbuf[100], dbuf[100];

    if (argc < 3) {
        printf("Please enter the http port, www resource path, and the log file name\n");
        return;
    }

    http_port = atoi(argv[1]);
    www_path = argv[2];
    log_file = argv[3];

    log_write("Using port %i, with www path %s, and log file %s\n", http_port, www_path, log_file);

    fprintf(stdout, "----- Lisod Server -----\n");

    /* all networked programs must create a socket */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) /*socket(int domain, int type, int protocol); call creates an endpoint for communication and return a descriptor*/
    {
        log_write("Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT); /*htons makes sure that the numbers are stored in memory in network bytes order*/
    addr.sin_addr.s_addr = INADDR_ANY;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        close_socket(sock);
        log_write("Failed binding socket.\n");
        return EXIT_FAILURE;
    }


    if (listen(sock, 5) < 0)
    /*backlog - the number of allowed connections.*/
    {
        close_socket(sock);
        log_write("Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    for (i=0; i<FD_SETSIZE; i++) {
        client[i] = -1;
    }
    log_init(log_file);
    max_idx = -1;
    maxfd = sock; //initialize maxfd
    FD_ZERO(&master); // initialize descriptor to empty
    FD_SET(sock, &master); // add the current listening descriptor to the monitored set


    /* finally, loop waiting for input and then write it back */
    while (1) {
        read_fds = master;
        nready = select(maxfd + 1, &read_fds, NULL, NULL, NULL);
        if ( nready == -1) {
            perror("select");
            exit(4);
        }

        if (FD_ISSET(sock, &read_fds)) {
            cli_size = sizeof(cli_addr);
            client_sock = accept(sock, (struct sockaddr *) &cli_addr, &cli_size);
            /* Get the client address*/
            struct in_addr ipAddr = ((struct sockaddr_in*)&cli_addr)->sin_addr;

            /*Transform the ip address into strings.*/
            char str[INET_ADDRSTRLEN];

            log_write("Client IP address is %s\n", inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN ));

            nready--;
            for (j = 0; j <= FD_SETSIZE; j++) {
                if (client[j] < 0) {
                    client[j] = client_sock;

                    FD_SET(client_sock, &master);
                    if (client_sock > maxfd) {
                        maxfd = client_sock;
                    }
                    if (j > max_idx) {
                        max_idx = j;
                    }
                    break;
                }
            }
        }
        else {
            for (k = 0; (k <= max_idx && nready > 0); k++) {
                if (client[k] > 0 && FD_ISSET(client[k], &read_fds)) {
                    nready--;
                    if ((readret = read(client[k], buf, BUF_SIZE)) > 1) {
                        log_write("Server received %d bytes data on %d\n", (int)readret, client[k]);

                        // handle request
                        Request *request = parse(buf, readret, client[k]);
                        stat(client[k], &sbuf);
                        tm = *gmtime(&sbuf.st_mtime); //st_mtime: time of last data modification.
                        //printf("yeah %d", mktime(tm));
                        if (&tm == 2){
                            now = time(0);
                            tm = *gmtime(&now);
                        }

                        strftime(tbuf, 1000, "%a, %d %b %Y %H:%M:%S %Z", &tm);
                        log_write("Last modified is %s\n", tbuf);
                        now = time(0);

                        tm = *gmtime(&now);
                        strftime(dbuf, 100, "%a, %d %b %Y %H:%M:%S %Z", &tm);
                        log_write("Date is %s\n", dbuf);


                        char * response = malloc(20000);

                        process_http_request(request, response, www_path);

                        printf("response is %s\n", response);
                        if (send(client[k], response, strlen(response), 0) < 0) {
                            close_socket(client[k]);
                            close_socket(sock);
                            log_write("Error sending to client.\n");
                            exit(EXIT_FAILURE);
                        }
                        log_write("Server sent %d bytes data to %d\n", strlen(response), client[k]);
                        free(response);
                    } else {
                        if (readret == 0) {
                            log_write("serve_clients: socket %d hung up\n", client[k]);
                        }
                        else {
                            log_write("serve_clients: recv return -1\n");
                        }
                        close_socket(client[k]);
                        FD_CLR(client[k], &master);
                        client[k] = -1;
                    }
                }
            }
        }
    }
}
