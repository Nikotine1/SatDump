#include "graphite.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Internal state (file-local)
static bool debug = false;
static char *graphite_server = nullptr;
static int graphite_port = 0;
static int graphite_sockfd = -1;

// Optional: setter functions
void set_graphite_server(const char *server, int port) {
    free(graphite_server); // avoid leak if already set
    graphite_server = strdup(server);
    graphite_port = port;
}

void set_graphite_debug(bool enable) {
    debug = enable;
}

// Connect function
int connect_graphite(void) {
    if (!graphite_server) return -1;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("graphite: socket()");
        return -1;
    }

    struct sockaddr_in servaddr = { .sin_family = AF_INET,
                                    .sin_port   = htons(graphite_port) };
    if (inet_pton(AF_INET, graphite_server, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "graphite: invalid address %s\n", graphite_server);
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("graphite: connect()");
        close(fd);
        return -1;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

    if (debug)
        fprintf(stderr, "graphite: connected to %s:%d\n",
                graphite_server, graphite_port);

    return fd;
}

// Send function
void send_to_graphite(const std::string &message) {
    if (graphite_sockfd < 0) {
        graphite_sockfd = connect_graphite();
        if (graphite_sockfd < 0) return;
    }

    ssize_t ret = send(graphite_sockfd, message.c_str(), message.size(), 0);
    if (ret <= 0) {
        if (errno == EPIPE || errno == ECONNRESET) {
            close(graphite_sockfd);
            graphite_sockfd = connect_graphite();
            if (graphite_sockfd >= 0) {
                send(graphite_sockfd, message.c_str(), message.size(), 0);
            }
        } else if (debug) {
            perror("graphite: send()");
        }
    }
}
