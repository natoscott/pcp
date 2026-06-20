/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-collectl socket server mode (-A).
 * Writes collectl-format output to a connected client, compatible with
 * pcp-colmux's legacy TCP socket protocol on port 2655.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "pcp-collectl.h"

#define COLLECTL_DEFAULT_PORT   2655

/*
 * Parse -A address[:port[:timeout]] or "server[:port]".
 * Returns 0 on success.
 */
static int
parse_address(const char *spec, char *host, size_t hlen,
              int *port, int *timeout, int *server_mode)
{
    char buf[256];
    char *p, *q;

    pmstrncpy(buf, sizeof(buf), spec);
    *port = COLLECTL_DEFAULT_PORT;
    *timeout = 10;
    *server_mode = 0;

    if (strncmp(buf, "server", 6) == 0) {
        long pv;
        *server_mode = 1;
        p = buf + 6;
        if (*p == ':') {
            pv = strtol(p + 1, NULL, 10);
            if (pv >= 1 && pv <= 65535) *port = (int)pv;
        }
        pmstrncpy(host, hlen, "0.0.0.0");
        return 0;
    }

    /* host[:port[:timeout]] */
    pmstrncpy(host, hlen, buf);
    if ((p = strchr(host, ':')) != NULL) {
        long pv, tv;
        *p = '\0';
        pv = strtol(p + 1, NULL, 10);
        if (pv >= 1 && pv <= 65535) *port = (int)pv;
        if ((q = strchr(p + 1, ':')) != NULL) {
            tv = strtol(q + 1, NULL, 10);
            if (tv > 0 && tv <= 3600) *timeout = (int)tv;
        }
    }
    return 0;
}

int
socket_server(collectl_ctx *ctx)
{
    char host[256];
    int port, timeout, server_mode;
    int sockfd, clientfd;
    struct sockaddr_in sa;
    socklen_t salen;
    FILE *client_fp;

    if (parse_address(ctx->address, host, sizeof(host),
                      &port, &timeout, &server_mode) < 0)
        return -1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("pcp-collectl: socket");
        return -1;
    }

    if (server_mode) {
        /* server mode: bind and wait for incoming connection */
        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        memset(&sa, 0, sizeof(sa));
        sa.sin_family      = AF_INET;
        sa.sin_addr.s_addr = INADDR_ANY;
        sa.sin_port        = htons((unsigned short)port);
        if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
            perror("pcp-collectl: bind");
            close(sockfd);
            return -1;
        }
        listen(sockfd, 1);
    server_loop:
        salen   = sizeof(sa);
        clientfd = accept(sockfd, (struct sockaddr *)&sa, &salen);
        if (clientfd < 0) {
            perror("pcp-collectl: accept");
            close(sockfd);
            return -1;
        }
    } else {
        /* client mode: connect to remote colmux */
        struct addrinfo hints, *res;
        char portstr[16];

        memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        pmsprintf(portstr, sizeof(portstr), "%d", port);

        if (getaddrinfo(host, portstr, &hints, &res) != 0 || res == NULL) {
            fprintf(stderr, "pcp-collectl: cannot resolve host: %s\n", host);
            close(sockfd);
            return -1;
        }
        if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
            perror("pcp-collectl: connect");
            freeaddrinfo(res);
            close(sockfd);
            return -1;
        }
        freeaddrinfo(res);
        clientfd = sockfd;
    }

    client_fp = fdopen(clientfd, "w");
    if (!client_fp) {
        close(clientfd);
        if (server_mode) close(sockfd);
        return -1;
    }

    /* redirect output to the socket and run the collection loop */
    int saved_stdout = dup(STDOUT_FILENO);
    dup2(clientfd, STDOUT_FILENO);
    collect_loop(ctx);
    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    fclose(client_fp);

    if (server_mode && !sigint_caught)
        goto server_loop;   /* await next connection in server mode */

    if (server_mode)
        close(sockfd);
    return 0;
}
