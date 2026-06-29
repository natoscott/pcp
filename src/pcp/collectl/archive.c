/*
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-collectl PCP archive writer using libpcp_import with PMI_APPEND.
 * Same approach as sysstat's sadc: archives grow day-long via append,
 * with metric dedup ensuring the .meta doesn't bloat on each session.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "pcp-collectl.h"

static int arch_ctx = -1;
static char arch_path[MAXPATHLEN];  /* current archive path */
char arch_path_last[MAXPATHLEN];    /* path of most recently closed archive */

/*
 * Build a space-separated list of active subsystem names from the bitmask,
 * e.g. "CPU DISK MEMORY NET SOCK" for pcp(1) to display via pmimport.args.
 */
static void
build_subsys_string(unsigned int subsys, char *buf, size_t len)
{
    unsigned int s;
    int first = 1;

    buf[0] = '\0';
    for (s = 0; s < collectl_nsubsys; s++) {
        const subsys_def *sd = &collectl_subsys[s];
        if (!(subsys & sd->ss_flag))
            continue;
        if (!first)
            strncat(buf, " ", len - strlen(buf) - 1);
        strncat(buf, sd->label, len - strlen(buf) - 1);
        first = 0;
    }
}

/*
 * Build archive path: <dir>/<hostname>-<YYYYMMDD>
 * If ctx->filename is set use it as the directory; otherwise use
 * COLLECTL_LOG_PATH/collectl.
 */
static void
build_archive_path(collectl_ctx *ctx, char *path, size_t len)
{
    char hostname[256];
    char datebuf[16];
    const char *dir;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    gethostname(hostname, sizeof(hostname));
    strftime(datebuf, sizeof(datebuf), "%Y%m%d", tm);

    dir = ctx->filename ? ctx->filename : COLLECTL_LOG_PATH;

    mkdir(dir, 0755);

    pmsprintf(path, len, "%s/%s-%s", dir, hostname, datebuf);
}

/*
 * pmiSetVolumeSize callback: compress a completed data volume.
 * Unlike other pmimport tools, pcp-collectl uses nanosleep for
 * timing (no alarm/setitimer), so no inherited timer to cancel.
 */
static void
volume_rotate_callback(const char *vol_path)
{
    pid_t pid = fork();

    if (pid == 0) {
        execlp("zstd", "zstd", "-q", "--rm", vol_path, (char *)NULL);
        _exit(1);
    }
}

static size_t
parse_volsize(const char *str)
{
    char *end;
    size_t val;

    if (str == NULL || *str == '\0')
        return 0;
    val = (size_t)strtoull(str, &end, 10);
    switch (tolower((unsigned char)*end)) {
    case 'g': val *= 1024; /* fall through */
    case 'm': val *= 1024; /* fall through */
    case 'k': val *= 1024; break;
    }
    return val;
}

int
archive_open(collectl_ctx *ctx)
{
    char path[MAXPATHLEN];
    char subsys_str[128];
    unsigned int s, m;
    pmLabelSet *sets = NULL;
    int nsets, i, j, saved;

    build_archive_path(ctx, path, sizeof(path));
    pmstrncpy(arch_path, sizeof(arch_path), path);

    arch_ctx = pmiStart(path, PMI_APPEND | PMI_PROCESS);
    if (pmiUseContext(arch_ctx) < 0) {
        fprintf(stderr, "pcp-collectl: cannot open archive %s: %s\n",
                path, pmiErrStr(arch_ctx));
        arch_ctx = -1;
        return -1;
    }

    /* annotate archive as collectl-created */
    pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, "collectl", "true");

    /* copy all context-level labels from the live local context */
    saved = pmWhichContext();
    pmUseContext(ctx->ctx);
    nsets = pmGetContextLabels(&sets);
    pmUseContext(saved);

    if (nsets > 0) {
        for (i = 0; i < nsets; i++) {
            for (j = 0; j < sets[i].nlabels; j++) {
                pmLabel *lp = &sets[i].labels[j];
                const char *name  = sets[i].json + lp->name;
                const char *value = sets[i].json + lp->value;
                char nbuf[64], vbuf[256];

                pmsprintf(nbuf, sizeof(nbuf), "%.*s",
                          (int)lp->namelen, name);
                pmsprintf(vbuf, sizeof(vbuf), "%.*s",
                          (int)lp->valuelen, value);
                if (strcmp(nbuf, "collectl") == 0)
                    continue;
                pmiPutLabel(PM_LABEL_CONTEXT, 0, 0, nbuf, vbuf);
            }
        }
        pmFreeLabelSets(sets, nsets);
    }

    /* register with pmdapmimport; pmiEnd() removes the sidecar automatically */
    build_subsys_string(ctx->subsys, subsys_str, sizeof(subsys_str));
    pmiSetImportProgram("collectl", PCP_COLLECTL_VERSION, subsys_str, arch_path);
    pmiSetZoneinfo(NULL);

    /* register all active subsystem metrics using pmDescs from local context */
    saved = pmWhichContext();
    pmUseContext(ctx->ctx);

    for (s = 0; s < collectl_nsubsys; s++) {
        const subsys_def *sd = &collectl_subsys[s];
        if (!(ctx->subsys & sd->ss_flag))
            continue;
        for (m = 0; m < sd->nmetrics; m++) {
            const metric_spec *ms = &sd->metrics[m];
            pmDesc desc;
            pmID pmid;
            const char *name = ms->name;
            char *text;

            if (pmLookupName(1, &name, &pmid) < 0)
                continue;
            if (pmLookupDesc(pmid, &desc) < 0)
                continue;

            pmUseContext(arch_ctx);
            pmiAddMetric(name, pmid, desc.type, desc.indom,
                         desc.sem, desc.units);

            pmUseContext(ctx->ctx);
            if (pmLookupText(pmid, PM_TEXT_ONELINE, &text) >= 0) {
                pmUseContext(arch_ctx);
                pmiPutText(PM_TEXT_PMID, PM_TEXT_ONELINE, pmid, text);
                free(text);
                pmUseContext(ctx->ctx);
            }
            if (pmLookupText(pmid, PM_TEXT_HELP, &text) >= 0) {
                pmUseContext(arch_ctx);
                pmiPutText(PM_TEXT_PMID, PM_TEXT_HELP, pmid, text);
                free(text);
                pmUseContext(ctx->ctx);
            }
        }
    }

    pmUseContext(arch_ctx);

    /* allocate PMI handles for fast per-sample writes */
    subsys_alloc_handles();

    /* enable intra-day volume rotation if LogVolSize is configured */
    {
        size_t volsize = parse_volsize(ctx->logvolsize);
        if (volsize > 0)
            pmiSetVolumeSize(volsize, volume_rotate_callback);
    }

    return 0;
}

void
archive_write(collectl_ctx *ctx)
{
    struct timespec ts;

    if (arch_ctx < 0)
        return;

    subsys_archive_write(ctx);

    clock_gettime(CLOCK_REALTIME, &ts);
    pmiWrite((unsigned long long)ts.tv_sec, (unsigned int)ts.tv_nsec);
}

void
archive_close(collectl_ctx *ctx)
{
    if (arch_ctx < 0)
        return;
    pmiEnd();   /* also removes the pmimport sidecar */
    arch_ctx = -1;
    pmstrncpy(arch_path_last, sizeof(arch_path_last), arch_path);
    arch_path[0] = '\0';
    (void)ctx;
}
