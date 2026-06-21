/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-colmux multi-host PCP fetch.
 * One pmNewContext(PM_CONTEXT_HOST) per host; uses subsys_mux_fetch()
 * for per-host pmFetch that stores results directly into h->vals[].
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
#include "pcp-collectl.h"

/* Number of metric slots — set after subsys_mux_lookup() */
static unsigned int g_nmetrics;

/*
 * Connect to all hosts.  Uses the first successful connection to build
 * the metric lookup table via subsys_mux_lookup(), then allocates per-host
 * value arrays.  Failures are non-fatal — host is marked unavailable.
 */
int
mux_connect_all(colmux_ctx *ctx)
{
    unsigned int i;
    int connected  = 0;
    int looked_up  = 0;

    g_nmetrics = 0;

    for (i = 0; i < ctx->nhosts; i++) {
        host_state *h = &ctx->hosts[i];

        h->ctx = pmNewContext(PM_CONTEXT_HOST, h->hostname);
        if (h->ctx < 0) {
            fprintf(stderr, "pcp-colmux: cannot connect to %s: %s\n",
                    h->hostname, pmErrStr(h->ctx));
            h->avail = 0;
            h->ctx   = -1;
            continue;
        }
        h->avail = 1;
        connected++;

        /* build the shared metric lookup table from first connected host */
        if (!looked_up) {
            subsys_mux_lookup(h->ctx);
            g_nmetrics = subsys_n_all();
            looked_up  = 1;
        }
    }

    if (g_nmetrics == 0)
        return connected > 0 ? 0 : -1;

    /* allocate per-host value and rate-calculation arrays */
    for (i = 0; i < ctx->nhosts; i++) {
        host_state *h = &ctx->hosts[i];
        h->vals      = calloc(g_nmetrics, sizeof(double));
        h->prev_vals = calloc(g_nmetrics, sizeof(double));
        if (!h->vals || !h->prev_vals) {
            fprintf(stderr, "pcp-colmux: out of memory\n");
            free(h->vals);
            free(h->prev_vals);
            h->vals = h->prev_vals = NULL;
            h->avail = 0;
        }
        /* initialise prev_vals to -1 (no previous sample) */
        if (h->prev_vals) {
            unsigned int j;
            for (j = 0; j < g_nmetrics; j++)
                h->prev_vals[j] = -1.0;
        }
    }

    return connected > 0 ? 0 : -1;
}

/*
 * Fetch one interval from all connected hosts into per-host h->vals[].
 * Rate calculation is done per-host against h->prev_vals[].
 */
int
mux_fetch_all(colmux_ctx *ctx)
{
    unsigned int i;
    int any = 0;

    for (i = 0; i < ctx->nhosts; i++) {
        host_state *h = &ctx->hosts[i];

        if (!h->avail || h->ctx < 0 || !h->vals)
            continue;

        if (subsys_mux_fetch(h->ctx, h->vals, g_nmetrics,
                             h->prev_vals, &h->prev_ts) < 0) {
            h->age++;
            if (h->age >= ctx->age_limit)
                h->avail = 0;
        } else {
            h->age = 0;
            any++;
        }
    }

    return any > 0 ? 0 : -1;
}

void
mux_disconnect_all(colmux_ctx *ctx)
{
    unsigned int i;

    for (i = 0; i < ctx->nhosts; i++) {
        host_state *h = &ctx->hosts[i];
        if (h->ctx >= 0) {
            pmDestroyContext(h->ctx);
            h->ctx   = -1;
            h->avail = 0;
        }
        free(h->vals);
        free(h->prev_vals);
        h->vals = h->prev_vals = NULL;
    }
}
