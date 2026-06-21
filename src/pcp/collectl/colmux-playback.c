/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-colmux playback mode: open a PCP archive per host, time-align
 * them and step through intervals together.
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

/*
 * Expand a playback filespec per host.
 * If the spec contains a '*', treat it as a glob pattern and assign
 * one matching archive per host in order.  Otherwise use the literal
 * path for all hosts (they must each have their own archive).
 *
 * Archive filename convention: hostname-YYYYMMDD.{meta,index,N}
 * We strip the extension for pmNewContext.
 */
static void
assign_playback_paths(colmux_ctx *ctx)
{
    unsigned int i;

    if (!strchr(ctx->playback, '*')) {
        /* same filespec for all hosts (each host's archive at same path) */
        for (i = 0; i < ctx->nhosts; i++) {
            ctx->hosts[i].ctx = pmNewContext(PM_CONTEXT_ARCHIVE,
                                             ctx->playback);
            ctx->hosts[i].avail = (ctx->hosts[i].ctx >= 0);
        }
        return;
    }

    /* glob expansion: match archives, assign one per host in order */
    {
        glob_t gl;
        char pattern[512];
        unsigned int gi;

        pmstrncpy(pattern, sizeof(pattern), ctx->playback);
        if (glob(pattern, 0, NULL, &gl) != 0) {
            fprintf(stderr, "pcp-colmux: no archives match: %s\n",
                    ctx->playback);
            return;
        }

        for (i = 0, gi = 0; i < ctx->nhosts && gi < gl.gl_pathc; i++, gi++) {
            /* strip known extensions to get archive base path */
            char base[512];
            const char *path = gl.gl_pathv[gi];
            const char *ext;

            pmstrncpy(base, sizeof(base), path);
            ext = strrchr(base, '.');
            if (ext && (strcmp(ext, ".meta") == 0 ||
                        strcmp(ext, ".index") == 0 ||
                        (ext[1] >= '0' && ext[1] <= '9')))
                *((char *)ext) = '\0';

            ctx->hosts[i].ctx = pmNewContext(PM_CONTEXT_ARCHIVE, base);
            ctx->hosts[i].avail = (ctx->hosts[i].ctx >= 0);
        }
        globfree(&gl);
    }
}

/*
 * Run the playback loop: step all archive contexts forward together,
 * display each aligned interval, sleep -delay between them.
 */
int
colmux_playback(colmux_ctx *ctx)
{
    unsigned int i;
    int any_active;
    struct timespec origin = { 0, 0 };

    assign_playback_paths(ctx);

    /* set all archive contexts to forward mode from the start */
    for (i = 0; i < ctx->nhosts; i++) {
        host_state *h = &ctx->hosts[i];
        if (!h->avail || h->ctx < 0)
            continue;

        pmUseContext(h->ctx);
        pmSetMode(PM_MODE_FORW, (const pmTimespec *)&origin, 0);
    }

    do {
        any_active = 0;

        for (i = 0; i < ctx->nhosts; i++) {
            host_state *h = &ctx->hosts[i];
            pmResult *result;
            int saved;

            if (!h->avail || h->ctx < 0)
                continue;

            saved = pmWhichContext();
            pmUseContext(h->ctx);

            if (pmFetch(0, NULL, &result) < 0) {
                h->avail = 0;
            } else {
                pmFreeResult(result);
                any_active = 1;
            }

            pmUseContext(saved);
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
