/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-collectl terminal output — brief and verbose modes.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <stdio.h>
#include <time.h>
#include "pcp-collectl.h"

#define MAX_VALS    (4096 * 32) /* MAX_INSTS * MAX_METRICS */
#define MAX_INSTS   4096

static double   vals[MAX_VALS];
static char    *insts[MAX_INSTS];
static unsigned int ninst;

void
output_header(collectl_ctx *ctx)
{
    unsigned int s;

    /* collectl brief-mode header is printed inline per subsystem;
     * verbose mode uses the per-subsystem verbose_hdr */
    if (ctx->verbose)
        return;

    for (s = 0; s < collectl_nsubsys; s++) {
        const subsys_def *sd = &collectl_subsys[s];
        if (!(ctx->subsys & sd->ss_flag))
            continue;
        if (sd->brief_hdr)
            printf("%s\n", sd->brief_hdr);
    }
}

void
output_interval(collectl_ctx *ctx)
{
    unsigned int s;
    time_t now;
    struct tm *tm;
    char timebuf[32];

    now = time(NULL);
    tm  = localtime(&now);
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm);

    /* reprint header if due */
    if (ctx->header_repeat && ctx->header_count == 0)
        output_header(ctx);

    /* single pmFetch for all active subsystems */
    if (subsys_fetch_all(ctx) < 0)
        return;

    for (s = 0; s < collectl_nsubsys; s++) {
        const subsys_def *sd = &collectl_subsys[s];
        if (!(ctx->subsys & sd->ss_flag))
            continue;

        ninst = 0;
        if (subsys_fetch(ctx, sd, vals, insts, &ninst) < 0)
            continue;

        if (!ctx->verbose)
            subsys_output_brief(sd, vals, insts, ninst, ctx);
        else
            subsys_output_verbose(sd, vals, insts, ninst, ctx);
    }

    if (ctx->header_repeat) {
        ctx->header_count++;
        if (ctx->header_count >= ctx->header_repeat)
            ctx->header_count = 0;
    }
}
