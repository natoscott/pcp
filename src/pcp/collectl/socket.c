/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-collectl socket server mode (-A).
 *
 * Wire protocol (compatible with original collectl and pcp-colmux):
 *   Client mode: each output line is prefixed "hostname line\n"
 *                End of interval: ">>><<<\n"
 *   Server mode: lines sent without hostname prefix
 *                End of interval: ">>><<<\n"
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
#define INTERVAL_MARKER         ">>><<<\n"

static int      sock_fd  = -1;  /* connected socket for output */
static int      sock_server_mode;
static char     sock_hostname[256];

/*
 * Socket-aware output: write line to socket with hostname prefix (client mode)
 * or without (server mode), buffered.
 */
static void
sock_write_line(const char *line)
{
    char buf[1024];
    ssize_t n, written, len;

    if (sock_server_mode)
        pmsprintf(buf, sizeof(buf), "%s\n", line);
    else
        pmsprintf(buf, sizeof(buf), "%s %s\n", sock_hostname, line);

    len = (ssize_t)strlen(buf);
    written = 0;
    while (written < len) {
        n = write(sock_fd, buf + written, (size_t)(len - written));
        if (n <= 0)
            return;
        written += n;
    }
}

static void
sock_send_interval_marker(void)
{
    const char *marker = INTERVAL_MARKER;
    ssize_t n, written = 0, len = (ssize_t)strlen(marker);
    while (written < len) {
        n = write(sock_fd, marker + written, (size_t)(len - written));
        if (n <= 0) break;
        written += n;
    }
}

/*
 * Run one collection session writing to sock_fd.
 * Intercepts stdout via pipe to add protocol framing per interval.
 */
static void
run_socket_session(collectl_ctx *ctx)
{
    int pipefd[2];
    int saved_stdout;
    FILE *rfp;
    char line[2048];
    unsigned int interval_lines = 0;

    if (pipe(pipefd) < 0) {
        perror("pcp-collectl: pipe");
        return;
    }

    saved_stdout = dup(STDOUT_FILENO);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);

    /* non-blocking reader */
    rfp = fdopen(pipefd[0], "r");

    /* run collect_loop in this thread — it writes to the pipe (stdout) */
    /* We use a simple approach: read lines as they arrive */
    /* For proper operation, we need a separate thread or select() */

    /* Simple synchronous approach: collect one interval, flush to socket */
    {
        unsigned int count_save = ctx->count;

        /* Collect one interval at a time, passing lines to socket */
        while (!sigint_caught && ctx->count != 0) {
            /* collect exactly one interval's worth of output */
            ctx->count = 1;
            collect_loop(ctx);
            fflush(stdout);

            /* read all lines from pipe and forward with protocol framing */
            while (fgets(line, sizeof(line), rfp)) {
                size_t len = strlen(line);
                if (len > 0 && line[len-1] == '\n')
                    line[len-1] = '\0';
                sock_write_line(line);
                interval_lines++;
            }
            sock_send_interval_marker();
            interval_lines = 0;

            if (count_save > 0)
                count_save--;
            if (count_save == 0 || count_save == (unsigned int)-1)
                break;
        }
        ctx->count = count_save;
    }

    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
    fclose(rfp);
}

/*
 * Parse -A address[:port[:timeout]] or "server[:port]".
 */
static int
parse_address(const char *spec, char *host, size_t hlen,
              int *port, int *timeout, int *server_mode)
{
    char buf[256];
    char *p, *q;
    long pv, tv;

    pmstrncpy(buf, sizeof(buf), spec);
    *port        = COLLECTL_DEFAULT_PORT;
    *timeout     = 10;
    *server_mode = 0;

    if (strncmp(buf, "server", 6) == 0) {
        *server_mode = 1;
        p = buf + 6;
        if (*p == ':') {
            pv = strtol(p + 1, NULL, 10);
            if (pv >= 1 && pv <= 65535) *port = (int)pv;
        }
        pmstrncpy(host, hlen, "0.0.0.0");
        return 0;
    }

    pmstrncpy(host, hlen, buf);
    if ((p = strchr(host, ':')) != NULL) {
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
    int bind_sockfd = -1, clientfd;
    struct sockaddr_in sa;
    socklen_t salen;

    if (parse_address(ctx->address, host, sizeof(host),
                      &port, &timeout, &server_mode) < 0)
        return -1;

    /* get our hostname for the wire protocol prefix */
    gethostname(sock_hostname, sizeof(sock_hostname));
    sock_server_mode = server_mode;

    if (server_mode) {
        int opt = 1;
        bind_sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (bind_sockfd < 0) { perror("pcp-collectl: socket"); return -1; }
        setsockopt(bind_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        memset(&sa, 0, sizeof(sa));
        sa.sin_family      = AF_INET;
        sa.sin_addr.s_addr = INADDR_ANY;
        sa.sin_port        = htons((unsigned short)port);
        if (bind(bind_sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
            perror("pcp-collectl: bind"); close(bind_sockfd); return -1;
        }
        listen(bind_sockfd, 4);

server_loop:
        salen    = sizeof(sa);
        clientfd = accept(bind_sockfd, (struct sockaddr *)&sa, &salen);
        if (clientfd < 0) {
            if (sigint_caught) goto done;
            perror("pcp-collectl: accept");
            goto done;
        }
        sock_fd = clientfd;
        run_socket_session(ctx);
        close(clientfd);
        sock_fd = -1;

        if (!sigint_caught)
            goto server_loop;

done:
        close(bind_sockfd);
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
            return -1;
        }

        clientfd = socket(res->ai_family, res->ai_socktype, 0);
        if (clientfd < 0) {
            perror("pcp-collectl: socket");
            freeaddrinfo(res);
            return -1;
        }
        if (connect(clientfd, res->ai_addr, res->ai_addrlen) < 0) {
            perror("pcp-collectl: connect");
            freeaddrinfo(res);
            close(clientfd);
            return -1;
        }
        freeaddrinfo(res);
        sock_fd = clientfd;
        run_socket_session(ctx);
        close(clientfd);
        sock_fd = -1;
    }

    return 0;
}
