#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#define FREEADDRINFO(r) do { freeaddrinfo(r); (r) = NULL; } while(0)

static const struct addrinfo HINTS = {
    .ai_family = AF_UNSPEC,
    .ai_socktype = SOCK_STREAM,
    .ai_flags = AI_PASSIVE | AI_V4MAPPED | AI_ADDRCONFIG,
    .ai_protocol = 0,
    .ai_addrlen = 0,
    .ai_addr = NULL,
    .ai_canonname = NULL,
    .ai_next = NULL
};

static const char *const DEFAULT_HOST = "::";
static const char *const DEFAULT_PORT = "8009";
static const int BACKLOG = 10;
static const int YES = 1;

int main (const int argc, const char *const argv[]) {
    struct addrinfo *res;
    struct addrinfo *res_p;
    int rv;
    int srv_fd;
    fd_set srv_fd_set;

    char ni_host[NI_MAXHOST];
    char ni_serv[NI_MAXSERV];

    struct sockaddr_storage remote_addr_storage;
    struct sockaddr *const remote_addr = (struct sockaddr *) &remote_addr_storage;
    socklen_t remote_addr_len;

    const char *const host = argc > 1 ? argv[1] : DEFAULT_HOST;
    const char *const port = argc > 2 ? argv[2] : DEFAULT_PORT;

    if ((rv = getaddrinfo(host, port, NULL, &res)) != 0) {
        fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(rv));
        goto main_err0;
    }

    /* socket() bind() listen() */
    for (res_p = res; res_p != NULL; res_p = res_p->ai_next) {
        if ((rv = getnameinfo(res_p->ai_addr, res_p->ai_addrlen, ni_host,
                        NI_MAXHOST, ni_serv, NI_MAXSERV, NI_NUMERICHOST
                        | NI_NUMERICSERV)) == 0) {
            if (strchr(ni_host, ':') == NULL) {
                /* IPv4 address */
                fprintf(stderr, "Trying %s:%s\n", ni_host, ni_serv);
            } else {
                /* IPv6 literal */
                fprintf(stderr, "Trying [%s]:%s\n", ni_host, ni_serv);
            }
        } else {
            if (rv == EAI_SYSTEM) {
                fprintf(stderr, "getnameinfo() failed: system error: %s\n", strerror(errno));
            } else {
                fprintf(stderr, "getnameinfo() failed: %s\n", gai_strerror(rv));
            }
            goto main_err1;
        }

        if ((srv_fd = socket(res_p->ai_family, res_p->ai_socktype, res_p->ai_protocol)) < 0) {
            fprintf(stderr, "socket() failed: %s\n", strerror(errno));
        } else if (setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &YES, sizeof(YES)) == -1) {
            fprintf(stderr, "setsockopt() failed: %s\n", strerror(errno));
            if (close(srv_fd) == -1) {
                fprintf(stderr, "close() failed: %s\n", strerror(errno));
                goto main_err1;
            }
        } else if (bind(srv_fd, res_p->ai_addr, res_p->ai_addrlen) == -1) {
            fprintf(stderr, "bind() failed: %s\n", strerror(errno));
            if (close(srv_fd) == -1) {
                fprintf(stderr, "close() failed: %s\n", strerror(errno));
                goto main_err1;
            }
        } else if (listen(srv_fd, BACKLOG) == -1) {
            fprintf(stderr, "listen() failed: %s\n", strerror(errno));
            if (close(srv_fd) == -1) {
                fprintf(stderr, "close() failed: %s\n", strerror(errno));
                goto main_err1;
            }
        } else {
            /* Everything worked */
            break;
        }
    }

    if (srv_fd < 0) { goto main_err1; }


    while(1) {
        FD_ZERO(&srv_fd_set);
        FD_SET(srv_fd, &srv_fd_set);
        if ((rv = select(srv_fd+1, &srv_fd_set, NULL, NULL, NULL)) == -1) {
            fprintf(stderr, "select() failed: %s\n", strerror(errno)); 
            goto main_err2;
        } else if (rv > 0) {
            if (FD_ISSET(srv_fd, &srv_fd_set)) {
                remote_addr_len = sizeof(remote_addr_storage);
                const int fd = accept(srv_fd, remote_addr, &remote_addr_len);
                if (fd == -1) {
                    fprintf(stderr, "accept() failed: %s\n", strerror(errno)); 
                    goto main_err2;
                }

                if ((rv = getnameinfo(remote_addr, remote_addr_len, ni_host,
                                NI_MAXHOST, ni_serv, NI_MAXSERV, NI_NUMERICHOST
                                | NI_NUMERICSERV)) == 0) {
                    if (strchr(ni_host, ':') == NULL) {
                        /* IPv4 address */
                        fprintf(stderr, "%s:%s Connected...", ni_host, ni_serv);
                    } else {
                        /* IPv6 literal */
                        fprintf(stderr, "[%s]:%s Connected...", ni_host, ni_serv);
                    }
                } else {
                    if (rv == EAI_SYSTEM) {
                        fprintf(stderr, "getnameinfo() failed: system error: %s\n", strerror(errno));
                    } else {
                        fprintf(stderr, "getnameinfo() failed: %s\n", gai_strerror(rv));
                    }
                    if (close(fd) == -1) {
                        fprintf(stderr, "close() failed: %s\n", strerror(errno));
                    }
                    goto main_err2;
                }

                if (close(fd) == -1) {
                    fprintf(stderr, "close() failed: %s\n", strerror(errno));
                    goto main_err2;
                }
                fprintf(stderr, " closed\n");
            }
        }
    }

    close(srv_fd);
    FREEADDRINFO(res);
    return EXIT_SUCCESS;

main_err2:
    if (close(srv_fd) == -1) {
        fprintf(stderr, "close() failed: %s\n", strerror(errno));
    }
main_err1:
    FREEADDRINFO(res);
main_err0:
    return EXIT_FAILURE;
}

/* vim: set ts=4 sw=4 sts et ai: */
