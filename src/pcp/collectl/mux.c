/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-colmux multi-host PCP fetch.
 * One pmNewContext(PM_CONTEXT_HOST) per host; single pmFetch per host
 * per interval using the same subsys metric tables as pcp-collectl.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pcp-colmux.h"

/* Shared subsystem table from pcp-collectl */
#include "pcp-collectl.h"

/* reuse the same metric lookup state */
extern int subsys_lookup(collectl_ctx *ctx);
extern int subsys_fetch_all(collectl_ctx *ctx);

/*
 * Connect to all hosts.  Failures are non-fatal — host is marked unavailable.
 * Returns number of successfully connected hosts.
 */
int
mux_connect_all(colmux_ctx *ctx)
{
    unsigned int i;
    int connected = 0;

    for (i = 0; i < ctx->nhosts; i++) {
        host_state *h = &ctx->hosts[i];

        h->ctx = pmNewContext(PM_CONTEXT_HOST, h->hostname);
        if (h->ctx < 0) {
            fprintf(stderr, "pcp-colmux: cannot connect to %s: %s\n",
                    h->hostname, pmErrStr(h->ctx));
            h->avail = 0;
            h->ctx   = -1;
        } else {
            h->avail = 1;
            connected++;
        }
    }
    return connected;
}

/*
 * Fetch one interval from all connected hosts.
 * Uses pmUseContext to switch between per-host contexts.
 * Increments age for hosts that fail; marks unavailable after age_limit.
 */
int
mux_fetch_all(colmux_ctx *ctx)
{
    unsigned int i;
    int any = 0;
    collectl_ctx cctx;      /* minimal collectl context for subsys machinery */
    struct timespec now;

    clock_gettime(CLOCK_REALTIME, &now);

    for (i = 0; i < ctx->nhosts; i++) {
        host_state *h = &ctx->hosts[i];
        pmResult *result;
        int saved, m;

        if (!h->avail || h->ctx < 0)
            continue;

        saved = pmWhichContext();
        pmUseContext(h->ctx);

        /*
         * Build a minimal collectl_ctx so we can call the shared
         * subsys lookup/fetch machinery.  Only ctx and prev_ts matter.
         */
        memset(&cctx, 0, sizeof(cctx));
        cctx.ctx     = h->ctx;
        cctx.prev_ts = h->prev_ts;

        /* subsys_fetch_all() does a single pmFetch across all registered metrics */
        if (subsys_fetch_all(&cctx) < 0) {
            h->age++;
            if (h->age >= ctx->age_limit)
                h->avail = 0;
        } else {
            h->age    = 0;
            h->prev_ts = now;
            any++;
        }

        pmUseContext(saved);
    }

    return any > 0 ? 0 : -1;
}

void
mux_disconnect_all(colmux_ctx *ctx)
{
    unsigned int i;

    for (i = 0; i < ctx->nhosts; i++) {
        if (ctx->hosts[i].ctx >= 0) {
            pmDestroyContext(ctx->hosts[i].ctx);
            ctx->hosts[i].ctx   = -1;
            ctx->hosts[i].avail = 0;
        }
    }
}
