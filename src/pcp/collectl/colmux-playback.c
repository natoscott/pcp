/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-colmux playback mode: open a PCP archive per host, time-align
 * them, and step through intervals together.
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
#include <glob.h>
#include "pcp-colmux.h"
#include "pcp-collectl.h"

/*
 * Assign archive paths to hosts.  Supports:
 *   - Literal path: used for all hosts (each host's archive at that path)
 *   - Glob pattern: one matching archive per host in sorted order
 * Sets h->ctx to the archive context and allocates h->vals[].
 */
static int
assign_playback_paths(colmux_ctx *ctx)
{
    unsigned int i;
    unsigned int nmetrics;
    int looked_up = 0;

    if (!strchr(ctx->playback, '*')) {
        /* literal: same archive for each host */
        for (i = 0; i < ctx->nhosts; i++) {
            ctx->hosts[i].ctx = pmNewContext(PM_CONTEXT_ARCHIVE,
                                             ctx->playback);
            ctx->hosts[i].avail = (ctx->hosts[i].ctx >= 0);
        }
    } else {
        /* glob: assign one archive per host in order */
        glob_t gl;
        unsigned int gi;

        memset(&gl, 0, sizeof(gl));
        if (glob(ctx->playback, 0, NULL, &gl) != 0) {
            fprintf(stderr, "pcp-colmux: no archives match: %s\n",
                    ctx->playback);
            return -1;
        }

        for (i = 0, gi = 0; i < ctx->nhosts && gi < gl.gl_pathc; i++, gi++) {
            char base[512];
            const char *path = gl.gl_pathv[gi];
            const char *ext;

            pmstrncpy(base, sizeof(base), path);
            ext = strrchr(base, '.');
            if (ext && (strcmp(ext, ".meta") == 0 ||
                        strcmp(ext, ".index") == 0 ||
                        (ext[1] >= '0' && ext[1] <= '9')))
                *((char *)ext) = '\0';

            ctx->hosts[i].ctx   = pmNewContext(PM_CONTEXT_ARCHIVE, base);
            ctx->hosts[i].avail = (ctx->hosts[i].ctx >= 0);
        }
        globfree(&gl);
    }

    /* build metric lookup from first available host */
    for (i = 0; i < ctx->nhosts && !looked_up; i++) {
        if (ctx->hosts[i].avail) {
            subsys_mux_lookup(ctx->hosts[i].ctx);
            looked_up = 1;
        }
    }

    nmetrics = subsys_n_all();
    if (nmetrics == 0)
        return -1;

    /* allocate per-host value arrays */
    for (i = 0; i < ctx->nhosts; i++) {
        host_state *h = &ctx->hosts[i];
        unsigned int j;

        h->vals      = calloc(nmetrics, sizeof(double));
        h->prev_vals = calloc(nmetrics, sizeof(double));
        if (!h->vals || !h->prev_vals) {
            free(h->vals);
            free(h->prev_vals);
            h->vals = h->prev_vals = NULL;
            h->avail = 0;
            continue;
        }
        for (j = 0; j < nmetrics; j++)
            h->prev_vals[j] = -1.0;

        /* position each archive context at the start */
        if (h->avail) {
            struct timespec origin = { 0, 0 };
            pmUseContext(h->ctx);
            pmSetMode(PM_MODE_FORW, (const pmTimespec *)&origin, 0);
        }
    }

    return looked_up ? 0 : -1;
}

/*
 * Playback loop: step all archive contexts forward together, display
 * each aligned interval, sleep -delay between them.
 */
int
colmux_playback(colmux_ctx *ctx)
{
    unsigned int i;
    int any_active;
    unsigned int nmetrics;

    if (assign_playback_paths(ctx) < 0) {
        fprintf(stderr, "pcp-colmux: no archive contexts available\n");
        return 1;
    }

    nmetrics = subsys_n_all();

    do {
        any_active = 0;

        for (i = 0; i < ctx->nhosts; i++) {
            host_state *h = &ctx->hosts[i];

            if (!h->avail || h->ctx < 0 || !h->vals)
                continue;

            if (subsys_mux_fetch(h->ctx, h->vals, nmetrics,
                                 h->prev_vals, &h->prev_ts) == PM_ERR_EOL) {
                h->avail = 0;   /* this host's archive is exhausted */
            } else {
                any_active = 1;
            }
        }

        if (any_active) {
            if (ctx->cols_mode)
                display_columnar(ctx);
            else
                display_sorted(ctx);

            if (ctx->delay > 0)
                usleep((useconds_t)(ctx->delay * 1e6));
        }

    } while (any_active);

    mux_disconnect_all(ctx);
    return 0;
}
