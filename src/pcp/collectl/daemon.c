/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-collectl daemonisation, signal handling and log rotation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
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
    pid_t pid;
    int fd;
    char pidbuf[32];
    FILE *fp;

    (void)ctx;

    pid = fork();
    if (pid < 0) {
        perror("pcp-collectl: fork");
        return -1;
    }
    if (pid > 0)
        exit(0);    /* parent exits */

    if (setsid() < 0) {
        perror("pcp-collectl: setsid");
        return -1;
    }

    pid = fork();   /* second fork: prevent re-acquiring a terminal */
    if (pid < 0) {
        perror("pcp-collectl: fork2");
        return -1;
    }
    if (pid > 0)
        exit(0);

    /* redirect stdio */
    fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO)
            close(fd);
    }

    /* write PID file */
    if ((fp = fopen(COLLECTL_PIDFILE, "w")) != NULL) {
        snprintf(pidbuf, sizeof(pidbuf), "%ld\n", (long)getpid());
        fputs(pidbuf, fp);
        fclose(fp);
    }

    return 0;
}

void
rotate_logs(collectl_ctx *ctx)
{
    /* archive_close + archive_open with a new timestamp-based path */
    archive_close(ctx);
    archive_open(ctx);
}
