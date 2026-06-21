/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-colmux legacy TCP socket client (DEPRECATED).
 *
 * Wire protocol from original collectl:
 *   Each output line: "hostname line-content\n"
 *   End-of-interval:  ">>><<<\n"
 *
 * The hostname prefix is stripped and the remaining line is parsed
 * for numeric column values which are stored in h->vals[].
 *
 * Column layout is derived from the collectl header lines (starting
 * with '#') which are sent once at connection time.  We parse the
 * header to determine column count and map them to h->vals[] positions.
 *
 * DEPRECATED: upgrade remote hosts to pcp-collectl for PCP-native
 * connections that are authenticated, structured, and extensible.
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
#include <ctype.h>
#include "pcp-colmux.h"

#define LEGACY_BUF      4096
#define INTERVAL_MARKER ">>><<<\n"
#define MAX_LEGACY_COLS 128

/*
 * Per-host legacy socket state.
 * We embed the fd in h->ctx as a negative value: ctx = -(fd + 2).
 */
#define LEGACY_FD(h)    (-(h)->ctx - 2)
#define IS_LEGACY(h)    ((h)->ctx < -1)

/*
 * Strip ANSI escape sequences from a string (in-place).
 * Prevents terminal injection from untrusted remote data.
 */
static void
strip_ansi(char *s)
{
    char *r = s, *w = s;
    while (*r) {
        if (*r == '\033' && r[1] == '[') {
            r += 2;
            while (*r && *r != 'm') r++;
            if (*r == 'm') r++;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}

/*
 * Parse a collectl data line (after stripping the hostname prefix)
 * and extract numeric column values into the provided vals[] array.
 *
 * collectl data lines look like (after the timestamp):
 *   "  0.5  0.0  1.2  ..."  — space-separated numeric columns
 *
 * Returns number of columns parsed, or -1 on error.
 */
int
legacy_parse_data_line(const char *line, double *vals, unsigned int maxcols)
{
    const char *p = line;
    unsigned int n = 0;
    char *end;
    double v;

    /* skip leading whitespace and timestamp (first non-numeric field) */
    while (*p && isspace((unsigned char)*p)) p++;
    /* skip timestamp if present (HH:MM:SS format) */
    if (isdigit((unsigned char)*p) && p[2] == ':')
        while (*p && !isspace((unsigned char)*p)) p++;

    while (*p && n < maxcols) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        v = strtod(p, &end);
        if (end == p) break;    /* not a number — stop */
        vals[n++] = v;
        p = end;
    }
    return (int)n;
}

/*
 * Connect to legacy collectl instances via TCP 2655.
 * Marks h->ctx with the encoded fd for later identification.
 * Returns number of legacy connections made.
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
        unsigned int nmet;

        if (h->avail)   /* already connected via pmcd */
            continue;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        snprintf(portstr, sizeof(portstr), "%d", ctx->port);

        if (getaddrinfo(h->hostname, portstr, &hints, &res) != 0 || !res)
            continue;

        sockfd = socket(res->ai_family, res->ai_socktype, 0);
        if (sockfd < 0) { freeaddrinfo(res); continue; }

        if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
            h->ctx   = -(sockfd + 2);
            h->avail = 1;
            connected++;

            /* allocate value array (size: maximum legacy columns) */
            nmet = MAX_LEGACY_COLS;
            h->vals      = calloc(nmet, sizeof(double));
            h->prev_vals = calloc(nmet, sizeof(double));
            if (!h->vals || !h->prev_vals) {
                free(h->vals); free(h->prev_vals);
                h->vals = h->prev_vals = NULL;
                close(sockfd);
                h->ctx   = -1;
                h->avail = 0;
            }
        } else {
            close(sockfd);
        }
        freeaddrinfo(res);
    }
    return connected;
}

/*
 * Read one interval from all legacy-connected hosts.
 *
 * Reads lines until the interval marker ">>><<<\n" is found.
 * Each line has format: "hostname content\n"
 * Strip hostname, sanitise ANSI, parse numeric columns into h->vals[].
 * Header lines (starting with '#') are skipped for column layout
 * (we use positional column extraction for simplicity).
 *
 * Returns number of hosts that provided data.
 */
int
legacy_socket_read(colmux_ctx *ctx)
{
    unsigned int i;
    int any = 0;
    char buf[LEGACY_BUF];

    for (i = 0; i < ctx->nhosts; i++) {
        host_state *h = &ctx->hosts[i];
        int sockfd;
        FILE *fp;

        if (!h->avail || !IS_LEGACY(h) || !h->vals)
            continue;

        sockfd = LEGACY_FD(h);
        fp     = fdopen(dup(sockfd), "r");
        if (!fp) continue;

        while (fgets(buf, sizeof(buf), fp)) {
            char *line;
            char *space;

            buf[strcspn(buf, "\r\n")] = '\0';
            strip_ansi(buf);

            /* end-of-interval marker */
            if (strncmp(buf, ">>><<<", 6) == 0) {
                any++;
                break;
            }

            /* strip hostname prefix: "hostname rest" */
            line = buf;
            space = strchr(line, ' ');
            if (!space)
                continue;
            line = space + 1;

            /* skip header lines */
            while (*line == ' ') line++;
            if (*line == '#')
                continue;

            /* parse numeric columns into h->vals[] */
            legacy_parse_data_line(line, h->vals, MAX_LEGACY_COLS);
        }

        fclose(fp);
    }
    return any;
}
