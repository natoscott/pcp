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
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "import.h"
#include "pcp-collectl.h"

static int arch_ctx = -1;
static char arch_path[MAXPATHLEN];  /* saved for pmimport sidecar */

/*
 * Write PCP_RUN_DIR/pmimport/collectl for pmdapmcd to serve as
 * pmcd.pmimport.{archive,version,args}.  Uses open()+fdopen() to
 * pin permissions to 0644 regardless of umask.
 */
static void
write_pmimport_sidecar(collectl_ctx *ctx, const char *archive_path)
{
    char dir[MAXPATHLEN];
    char sidecar[MAXPATHLEN];
    const char *rundir;
    int fd;
    FILE *fp;

    rundir = pmGetConfig("PCP_RUN_DIR");

    pmsprintf(dir, sizeof(dir), "%s/pmimport", rundir);
    mkdir(dir, 0755);

    pmsprintf(sidecar, sizeof(sidecar), "%s/collectl", dir);
    fd = open(sidecar, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
        return;
    if ((fp = fdopen(fd, "w")) == NULL) {
        close(fd);
        return;
    }
    fprintf(fp, "version=%s\n", PCP_COLLECTL_VERSION);
    fprintf(fp, "args=%s\n", ctx->opts[0] ? ctx->opts : "-s all");
    fprintf(fp, "archive=%s\n", archive_path);
    fclose(fp);
}

static void
remove_pmimport_sidecar(void)
{
    char sidecar[MAXPATHLEN];
    const char *rundir;

    rundir = pmGetConfig("PCP_RUN_DIR");
    pmsprintf(sidecar, sizeof(sidecar), "%s/pmimport/collectl", rundir);
    unlink(sidecar);
}

/*
 * Build archive path: <dir>/collectl/<hostname>/<YYYYMMDD>
 * If ctx->filename specifies a directory, use it; otherwise use
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

    dir = ctx->filename ? ctx->filename
                        : COLLECTL_LOG_PATH "/collectl";

    /* ensure directory exists */
    mkdir(dir, 0755);

    pmsprintf(path, len, "%s/%s-%s", dir, hostname, datebuf);
}

int
archive_open(collectl_ctx *ctx)
{
    char path[MAXPATHLEN];
    unsigned int s, m;
    pmLabelSet *sets = NULL;
    int nsets, i, j, saved;

    build_archive_path(ctx, path, sizeof(path));
    pmstrncpy(arch_path, sizeof(arch_path), path);

    arch_ctx = pmiStart(path, PMI_APPEND);
    if ((pmiUseContext(arch_ctx)) < 0) {
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
    write_pmimport_sidecar(ctx, arch_path);

    /* register all active subsystem metrics */
    for (s = 0; s < collectl_nsubsys; s++) {
        const subsys_def *sd = &collectl_subsys[s];
        if (!(ctx->subsys & sd->ss_flag))
            continue;
        for (m = 0; m < sd->nmetrics; m++) {
            const metric_spec *ms = &sd->metrics[m];
            pmDesc desc;
            pmID pmid;
            const char *name = ms->name;

            if (pmLookupName(1, &name, &pmid) < 0)
                continue;
            if (pmLookupDesc(pmid, &desc) < 0)
                continue;
            pmiAddMetric(name, pmid, desc.type, desc.indom,
                         desc.sem, desc.units);
        }
    }

    return 0;
}

void
archive_write(collectl_ctx *ctx)
{
    struct timespec ts;

    if (arch_ctx < 0)
        return;

    clock_gettime(CLOCK_REALTIME, &ts);
    pmiHighResWrite(ts.tv_sec, ts.tv_nsec);
}

void
archive_close(collectl_ctx *ctx)
{
    if (arch_ctx < 0)
        return;
    pmiEnd();
    arch_ctx = -1;
    remove_pmimport_sidecar();
    (void)ctx;
}
