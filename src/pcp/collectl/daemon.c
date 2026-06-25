/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-collectl daemonisation, signal handling, archive rotation,
 * post-rotation compression, and archive culling.
 *
 * pcp-collectl depends on pcp-libs + pcp-conf only — no pmlogger_daily.
 * All archive lifecycle management is self-contained here.
 *
 * Compression strategy (mirroring PCP conventions):
 *   .meta files   → xz   (high ratio; metadata read once at open)
 *   data volumes  → zstd (fast decompression for pmval/replay)
 *   .index files  → not compressed (tiny; needed for fast seeks)
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
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
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

    (void)ctx;

    pid = fork();
    if (pid < 0) {
        perror("pcp-collectl: fork");
        return -1;
    }
    if (pid > 0)
        exit(0);

    if (setsid() < 0) {
        perror("pcp-collectl: setsid");
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        perror("pcp-collectl: fork2");
        return -1;
    }
    if (pid > 0)
        exit(0);

    fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO)
            close(fd);
    }

    return 0;
}

/*
 * Compress a single file in a forked child using the given compressor.
 * The child replaces the original file with <file>.<suffix>.
 * Returns immediately — compression happens asynchronously.
 */
static void
compress_file_async(const char *path, const char *compressor,
                    const char *suffix)
{
    struct stat st;
    pid_t pid;

    if (stat(path, &st) < 0)
        return;

    pid = fork();
    if (pid < 0)
        return;

    if (pid == 0) {
        /* child: exec compressor, replace file in-place */
        execlp(compressor, compressor, "--rm", "-q", path, (char *)NULL);
        /* if exec fails, try with full path */
        _exit(1);
    }
    /* parent: don't wait — reap via SIGCHLD or next rotation */
    (void)suffix;
}

/*
 * Compress all volumes of a closed PCP archive:
 *   hostname-YYYYMMDD.meta  → xz
 *   hostname-YYYYMMDD.N     → zstd   (N = 0, 1, 2, ...)
 *   hostname-YYYYMMDD.index → left alone
 *
 * Run each compressor in a background child so the collect loop
 * is not blocked by I/O.
 */
static void
compress_archive(const char *base_path)
{
    char path[MAXPATHLEN];
    int vol;

    /* compress .meta with xz */
    pmsprintf(path, sizeof(path), "%s.meta", base_path);
    compress_file_async(path, "xz", "xz");

    /* compress data volumes with zstd until none found */
    for (vol = 0; vol < 1000; vol++) {
        pmsprintf(path, sizeof(path), "%s.%d", base_path, vol);
        if (access(path, F_OK) != 0)
            break;
        compress_file_async(path, "zstd", "zst");
    }
}

/*
 * Extract YYYYMMDD from a filename of the form: prefix-YYYYMMDD[-HHMMSS].*
 * Returns 0 on success, -1 if pattern not found.
 */
static int
extract_date_from_name(const char *name, char *datebuf, size_t len)
{
    const char *p;
    const char *dash;

    /* find last '-' before an 8-digit sequence */
    for (p = name; *p; p++) {
        if (*p == '-' && p[1] >= '0' && p[1] <= '9') {
            /* candidate: check for 8 consecutive digits */
            const char *q = p + 1;
            int i;
            for (i = 0; i < 8; i++)
                if (q[i] < '0' || q[i] > '9')
                    break;
            if (i == 8 && (q[8] == '.' || q[8] == '-' || q[8] == '\0')) {
                pmstrncpy(datebuf, len, q);
                datebuf[8] = '\0';
                return 0;
            }
        }
    }
    (void)dash;
    return -1;
}

/*
 * Cull old archives from the given directory.
 * Deletes any file/directory whose embedded YYYYMMDD is strictly
 * older than the cutoff date (today minus purgeDays).
 */
void
cull_old_archives(const char *dirpath, unsigned int purgeDays)
{
    DIR *dp;
    struct dirent *de;
    time_t cutoff;
    struct tm *tm;
    char cutdate[16];
    char namebuf[16];
    char fullpath[MAXPATHLEN];

    if (purgeDays == 0)
        return;

    cutoff = time(NULL) - (time_t)purgeDays * 86400;
    tm = localtime(&cutoff);
    strftime(cutdate, sizeof(cutdate), "%Y%m%d", tm);

    dp = opendir(dirpath);
    if (dp == NULL)
        return;

    while ((de = readdir(dp)) != NULL) {
        if (de->d_name[0] == '.')
            continue;
        /* skip message log files (handled separately by purgeMons) */
        if (strstr(de->d_name, ".log"))
            continue;

        if (extract_date_from_name(de->d_name, namebuf, sizeof(namebuf)) < 0)
            continue;

        /* string compare YYYYMMDD — works because format is fixed-width */
        if (strcmp(namebuf, cutdate) < 0) {
            pmsprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, de->d_name);
            unlink(fullpath);
        }
    }
    closedir(dp);
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
        next += 86400;      /* roll time already passed today — schedule tomorrow */

    if (rollMins > 0) {
        /* sub-day rotation: find next multiple of rollMins from midnight */
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
 * Rotate: close current archive, compress old, open new, cull old ones.
 * Called from collect.c when wall-clock >= next rotation time,
 * or when SIGUSR1 is received for a manual roll.
 */
void
rotate_logs(collectl_ctx *ctx)
{
    extern char arch_path_last[];   /* last archive path, saved in archive.c */
    char old_path[MAXPATHLEN];
    char dirpath[MAXPATHLEN];
    char *slash;

    pmstrncpy(old_path, sizeof(old_path), arch_path_last);

    archive_close(ctx);

    /* compress the archive we just closed */
    if (old_path[0] != '\0')
        compress_archive(old_path);

    archive_open(ctx);

    /* cull old archives from the same directory */
    pmstrncpy(dirpath, sizeof(dirpath), old_path);
    slash = strrchr(dirpath, '/');
    if (slash)
        *slash = '\0';
    else
        pmstrncpy(dirpath, sizeof(dirpath), ".");

    cull_old_archives(dirpath, ctx->purge_days);
}
