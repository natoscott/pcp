/*
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-collectl live collection loop.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "pcp-collectl.h"


static void
next_interval(struct timespec *next, unsigned int secs)
{
    next->tv_sec += secs;
}

static void
align_to_boundary(struct timespec *next, unsigned int secs)
{
    time_t now = time(NULL);
    next->tv_sec  = (now / secs + 1) * secs;
    next->tv_nsec = 0;
}

static void
sleep_until(const struct timespec *target)
{
    struct timespec now, rem;
    long nsec;

    clock_gettime(CLOCK_REALTIME, &now);
    rem.tv_sec  = target->tv_sec - now.tv_sec;
    rem.tv_nsec = target->tv_nsec - now.tv_nsec;
    if (rem.tv_nsec < 0) {
        rem.tv_sec--;
        rem.tv_nsec += 1000000000L;
    }
    if (rem.tv_sec < 0)
        return;         /* already past target */

    /* sleep in 100ms slices to remain responsive to signals */
    nsec = rem.tv_sec * 1000000000L + rem.tv_nsec;
    while (nsec > 0 && !sigint_caught) {
        struct timespec slice;
        slice.tv_sec  = 0;
        slice.tv_nsec = nsec < 100000000L ? nsec : 100000000L;
        nanosleep(&slice, NULL);
        nsec -= slice.tv_nsec;
    }
}

int
collect_loop(collectl_ctx *ctx)
{
    struct timespec next;
    unsigned int interval2_ticks = 0;
    unsigned int interval3_ticks = 0;
    unsigned int interval2_limit;
    unsigned int interval3_limit;

    clock_gettime(CLOCK_REALTIME, &next);

    if (ctx->align)
        align_to_boundary(&next, ctx->interval);

    /* how many primary ticks before secondary/tertiary collections */
    interval2_limit = (ctx->interval2 >= ctx->interval)
                          ? ctx->interval2 / ctx->interval : 1;
    interval3_limit = (ctx->interval3 >= ctx->interval)
                          ? ctx->interval3 / ctx->interval : 1;

    /* initialise rotation schedule if -r was given */
    if (ctx->roll_time[0] != '\0')
        ctx->next_roll = next_roll_time(ctx->roll_time, ctx->roll_mins);

    /* prime the pump: one silent fetch to initialise rate denominators */
    clock_gettime(CLOCK_REALTIME, &ctx->prev_ts);
    output_interval(ctx);   /* sets up prev values; first output skipped */

    while (!sigint_caught && ctx->count != 0) {
        next_interval(&next, ctx->interval);
        sleep_until(&next);

        if (sigint_caught)
            break;

        /* SIGUSR1 or scheduled rotation */
        if (sigusr1_caught ||
            (ctx->next_roll && time(NULL) >= ctx->next_roll)) {
            sigusr1_caught = 0;
            rotate_logs(ctx);
            if (ctx->roll_time[0] != '\0')
                ctx->next_roll = next_roll_time(ctx->roll_time, ctx->roll_mins);
        }

        /* determine if secondary (process/slab) or tertiary (env) tick */
        interval2_ticks++;
        interval3_ticks++;
        if (interval2_ticks >= interval2_limit) {
            interval2_ticks = 0;
            /* secondary subsystems (Z, y/Y) are always included in the
             * normal subsys_fetch pass below; their interval just means
             * we fetch them less often */
        }
        if (interval3_ticks >= interval3_limit)
            interval3_ticks = 0;

        /* fetch + output all active subsystems */
        output_interval(ctx);

        /* write to PCP archive if open */
        archive_write(ctx);

        if (ctx->count > 0)
            ctx->count--;
    }

    archive_close(ctx);
    return 0;
}
