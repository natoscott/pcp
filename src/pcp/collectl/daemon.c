/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-collectl signal handling and archive rotation.
 *
 * Archive lifecycle management (compression, culling) is handled by
 * collectl-daily, run via ExecStartPre and the collectl-daily timer.
 * Intra-day data volume rotation is handled by pmiSetVolumeSize().
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
#include <signal.h>
#include <time.h>
#include "pcp-collectl.h"

volatile sig_atomic_t sigint_caught;
volatile sig_atomic_t sigusr1_caught;

static void
sig_handler(int sig)
{
    if (sig == SIGTERM || sig == SIGINT)
        sigint_caught = 1;
    else if (sig == SIGUSR1)
        sigusr1_caught = 1;
}

void
setup_signals(void)
{
    struct sigaction sa;

    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
}

int
daemonise(collectl_ctx *ctx)
{
    (void)ctx;
    return 0;
}

/*
 * Compute the next rotation time in seconds since epoch.
 * rollTime is "HH:MM", rollMins is the sub-day interval (0 = daily).
 */
time_t
next_roll_time(const char *rollTime, unsigned int rollMins)
{
    struct tm *tm;
    time_t now;
    time_t next;
    int rollHour, rollMin;

    if (sscanf(rollTime, "%d:%d", &rollHour, &rollMin) != 2)
        return 0;

    now = time(NULL);
    tm  = localtime(&now);

    tm->tm_hour = rollHour;
    tm->tm_min  = rollMin;
    tm->tm_sec  = 0;
    next = mktime(tm);

    if (next <= now)
        next += 86400;

    if (rollMins > 0) {
        time_t midnight;
        struct tm m = *tm;
        m.tm_hour = m.tm_min = m.tm_sec = 0;
        midnight = mktime(&m);
        next = midnight;
        while (next <= now)
            next += (time_t)rollMins * 60;
    }

    return next;
}

/*
 * Rotate: close current archive and open a new one.
 * Called from collect.c when wall-clock >= next rotation time,
 * or when SIGUSR1 is received for a manual roll.
 * Compression and culling of prior-day archives is handled by
 * collectl-daily (run via ExecStartPre and daily timer).
 */
void
rotate_logs(collectl_ctx *ctx)
{
    archive_close(ctx);
    archive_open(ctx);
}
