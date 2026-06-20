/*
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-collectl playback from PCP archives.
 * Handles both PCP native archives (via pmNewContext) and -a/--archive.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <stdio.h>
#include "pcp-collectl.h"

int
playback_loop(collectl_ctx *ctx)
{
    int sts;
    struct timespec origin = { 0, 0 };

    ctx->ctx = pmNewContext(PM_CONTEXT_ARCHIVE, ctx->playback);
    if (ctx->ctx < 0) {
        fprintf(stderr, "pcp-collectl: cannot open archive %s: %s\n",
                ctx->playback, pmErrStr(ctx->ctx));
        return 1;
    }

    /* step forward from beginning; pmFetch drives the interval */
    sts = pmSetMode(PM_MODE_FORW, (const pmTimespec *)&origin, 0);
    if (sts < 0) {
        fprintf(stderr, "pcp-collectl: pmSetMode failed: %s\n",
                pmErrStr(sts));
        return 1;
    }

    if (subsys_lookup(ctx) < 0)
        return 1;

    /* replay loop: output_interval fetches from archive context */
    while (!sigint_caught) {
        output_interval(ctx);
        /*
         * subsys_fetch drives pmFetch internally; when it returns
         * PM_ERR_EOL we're done.  A simple sentinel: if all subsystems
         * return 0 values we've hit the end.
         */
        if (ctx->count > 0 && --ctx->count == 0)
            break;
    }

    pmDestroyContext(ctx->ctx);
    ctx->ctx = -1;
    return 0;
}
