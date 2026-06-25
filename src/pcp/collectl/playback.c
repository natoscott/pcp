/*
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-collectl playback from PCP archives.
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

    sts = pmSetMode(PM_MODE_FORW, &origin, 0);
    if (sts < 0) {
        fprintf(stderr, "pcp-collectl: pmSetMode failed: %s\n",
                pmErrStr(sts));
        return 1;
    }

    if (subsys_lookup(ctx) < 0)
        return 1;

    /*
     * Replay loop.  subsys_fetch_all() now propagates PM_ERR_EOL from
     * pmFetch so output_interval() returns it upward via a thread-local
     * variable.  We use a simpler approach: call subsys_fetch_all()
     * directly and stop on PM_ERR_EOL.
     */
    while (!sigint_caught) {
        int fetch_sts = subsys_fetch_all(ctx);

        if (fetch_sts == PM_ERR_EOL)
            break;      /* clean end of archive */

        if (fetch_sts < 0 && fetch_sts != PM_ERR_EOL)
            break;      /* other error */

        output_interval(ctx);

        if (ctx->count > 0 && --ctx->count == 0)
            break;
    }

    pmDestroyContext(ctx->ctx);
    ctx->ctx = -1;
    return 0;
}
