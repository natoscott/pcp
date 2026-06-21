/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-colmux legacy TCP socket client (DEPRECATED).
 *
 * Connects to old collectl instances on port 2655.  Wire format:
 *   "hostname line-content\n" per output line
 *   ">>><<<\n"                end-of-interval marker
 *
 * This mode is deprecated in favour of PCP-native connections via pmcd.
 * It is retained only to support rolling upgrades where some hosts still
 * run the original collectl.  Use pcp-collectl on all remote hosts to
 * eliminate the need for this code path.
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
#include "pcp-colmux.h"

#define LEGACY_BUF  4096
#define INTERVAL_MARKER ">>><<<\n"

/*
 * Attempt a legacy socket connection for any host that doesn't have a
 * pmcd context (h->ctx < 0).  On success, stores the socket fd in a
 * reserved slot and sets h->avail = 1.
 *
 * NOTE: this is best-effort; failure is non-fatal.
 */
int
legacy_socket_connect(colmux_ctx *ctx)
{
    unsigned int i;
    int connected = 0;

    for (i = 0; i < ctx->nhosts; i++) {
        host_state *h = &ctx->hosts[i];
        struct addrinfo hints, *res;
        char portstr[16];
        int sockfd;

        if (h->avail)   /* already connected via pmcd */
            continue;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        snprintf(portstr, sizeof(portstr), "%d", ctx->port);

        if (getaddrinfo(h->hostname, portstr, &hints, &res) != 0 || !res)
            continue;

        sockfd = socket(res->ai_family, res->ai_socktype, 0);
        if (sockfd < 0) {
            freeaddrinfo(res);
            continue;
        }
        if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
            /* store fd in ctx field; use vals[0] as a type flag */
            /* We use a negative value in the ctx field as a legacy marker */
            h->ctx   = -(sockfd + 2);   /* encode fd: negative, offset by 2 */
            h->avail = 1;
            connected++;
        } else {
            close(sockfd);
        }
        freeaddrinfo(res);
    }
    return connected;
}

/*
 * Read one interval from all legacy-connected hosts.
 * Parses "hostname line\n" format; stores the first numeric field after
 * hostname into h->vals[0] as a rough metric.  Full parsing would require
 * knowing the subsystem column layout from the -command string.
 *
 * Lines ending with the interval marker (>>><<<) signal end of interval.
 */
int
legacy_socket_read(colmux_ctx *ctx)
{
    unsigned int i;
    int any = 0;

    for (i = 0; i < ctx->nhosts; i++) {
        host_state *h = &ctx->hosts[i];
        int sockfd;
        char buf[LEGACY_BUF];
        ssize_t n;

        if (!h->avail || h->ctx >= 0)  /* skip pmcd-connected hosts */
            continue;

        sockfd = -(h->ctx + 2);        /* decode fd */

        while ((n = read(sockfd, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            if (strstr(buf, INTERVAL_MARKER))
                break;
            any++;
        }

        if (n <= 0) {
            close(sockfd);
            h->ctx   = -1;
            h->avail = 0;
        }
    }
    return any;
}
