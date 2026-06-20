/*
 * Copyright (c) 2026 Red Hat.
 *
 * Generic subsystem fetch, rate-calculation, and output machinery for
 * pcp-collectl.  All subsystems share this code; per-subsystem details
 * (metric names, column headers, indoms) live in subsys_defs.c.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <stdio.h>
#include "pcp-collectl.h"

#define MAX_INSTS   4096    /* max instances per subsystem */
#define MAX_METRICS 32      /* max metrics per subsystem */

/* Per-subsystem fetch state: pmIDs, descriptors, handles */
typedef struct {
    pmID    pmids[MAX_METRICS];
    pmDesc  descs[MAX_METRICS];
    int     nlooked;        /* number successfully looked up */
} ss_state;

static ss_state states[32]; /* one per entry in collectl_subsys[] */
static int      states_init;

/*
 * Look up metric names for all active subsystems.  Called once at
 * startup after the pmContext is established.
 */
int
subsys_lookup(collectl_ctx *ctx)
{
    unsigned int s, m;
    const subsys_def *sd;
    ss_state *st;
    const char *names[MAX_METRICS];

    for (s = 0; s < collectl_nsubsys; s++) {
        sd = &collectl_subsys[s];
        if (!(ctx->subsys & sd->ss_flag))
            continue;

        st = &states[s];
        st->nlooked = 0;

        for (m = 0; m < sd->nmetrics && m < MAX_METRICS; m++)
            names[m] = sd->metrics[m].name;

        if (pmLookupName(sd->nmetrics, names, st->pmids) < 0)
            continue;

        for (m = 0; m < sd->nmetrics; m++) {
            if (st->pmids[m] == PM_ID_NULL)
                continue;
            if (pmLookupDesc(st->pmids[m], &st->descs[m]) < 0)
                st->pmids[m] = PM_ID_NULL;
            else
                st->nlooked++;
        }
    }
    states_init = 1;
    return 0;
}

/*
 * Fetch one interval for a single subsystem.
 * Fills vals[] with (scaled, rate-calculated) double values.
 * For instanced metrics, fills inst_names[] and *ninst.
 * Returns number of values written, or <0 on error.
 */
int
subsys_fetch(collectl_ctx *ctx, const subsys_def *sd,
             double *vals, char **inst_names, unsigned int *ninst)
{
    unsigned int s, m, i;
    int sts;
    pmResult *result;
    ss_state *st;
    struct timespec now;
    double elapsed;

    if (!states_init)
        return -1;

    /* find this subsystem's index */
    for (s = 0; s < collectl_nsubsys; s++)
        if (&collectl_subsys[s] == sd)
            break;
    if (s >= collectl_nsubsys)
        return -1;

    st = &states[s];
    if (st->nlooked == 0)
        return 0;

    if ((sts = pmFetch(sd->nmetrics, st->pmids, &result)) < 0)
        return sts;

    clock_gettime(CLOCK_MONOTONIC, &now);
    elapsed = (now.tv_sec - ctx->prev_ts.tv_sec) +
              (now.tv_nsec - ctx->prev_ts.tv_nsec) * 1e-9;
    if (elapsed <= 0.0)
        elapsed = 1.0;

    *ninst = 0;

    for (m = 0; m < result->numpmid; m++) {
        pmValueSet *vset = result->vset[m];
        pmAtomValue atom;
        double raw;

        if (vset->numval <= 0)
            continue;

        /* find which metric spec this pmid corresponds to */
        unsigned int spec = 0;
        for (; spec < sd->nmetrics; spec++)
            if (st->pmids[spec] == vset->pmid)
                break;
        if (spec >= sd->nmetrics)
            continue;

        const metric_spec *ms = &sd->metrics[spec];

        if (sd->indom != PM_INDOM_NULL && vset->numval > 1) {
            /* instanced: one value per instance */
            for (i = 0; i < (unsigned int)vset->numval; i++) {
                char *iname = NULL;
                if (pmNameInDom(sd->indom, vset->vlist[i].inst,
                                &iname) < 0)
                    iname = "?";
                if (ms->flags & MSF_INSTANCED) {
                    if (*ninst < MAX_INSTS)
                        inst_names[*ninst] = iname;
                }
                if (pmExtractValue(vset->valfmt, &vset->vlist[i],
                                   st->descs[spec].type,
                                   &atom, PM_TYPE_DOUBLE) < 0)
                    continue;
                raw = atom.d;
                if (ms->flags & MSF_RATE)
                    raw /= elapsed;
                raw *= (ms->scale != 0.0 ? ms->scale : 1.0);
                vals[spec * MAX_INSTS + i] = raw;
                if (i + 1 > *ninst)
                    *ninst = i + 1;
            }
        } else {
            /* singular */
            if (pmExtractValue(vset->valfmt, &vset->vlist[0],
                               st->descs[spec].type,
                               &atom, PM_TYPE_DOUBLE) < 0)
                continue;
            raw = atom.d;
            if (ms->flags & MSF_RATE)
                raw /= elapsed;
            raw *= (ms->scale != 0.0 ? ms->scale : 1.0);
            vals[spec] = raw;
            if (*ninst == 0)
                *ninst = 1;
        }
    }

    pmFreeResult(result);
    ctx->prev_ts = now;
    return (int)*ninst;
}

/*
 * Print brief (one-line) output for a subsystem.
 * Mirrors the exact column layout from the original collectl.
 */
void
subsys_output_brief(const subsys_def *sd, double *vals,
                    char **insts, unsigned int ninst,
                    collectl_ctx *ctx)
{
    unsigned int m;

    if (sd->brief_hdr && ctx->header_count == 0)
        printf("%s\n", sd->brief_hdr);

    printf("%-8s ", sd->label);
    for (m = 0; m < sd->nmetrics; m++) {
        const metric_spec *ms = &sd->metrics[m];
        if (ms->flags & MSF_PCT)
            printf("%*.1f ", ms->col_width, vals[m]);
        else if (ms->flags & MSF_KB)
            printf("%*.0f ", ms->col_width, vals[m]);
        else
            printf("%*g ", ms->col_width, vals[m]);
    }
    putchar('\n');
}

/*
 * Print verbose (multi-line) output for a subsystem.
 */
void
subsys_output_verbose(const subsys_def *sd, double *vals,
                      char **insts, unsigned int ninst,
                      collectl_ctx *ctx)
{
    unsigned int i, m;

    if (sd->verbose_hdr)
        printf("%s\n", sd->verbose_hdr);

    if (sd->indom == PM_INDOM_NULL || ninst <= 1) {
        /* aggregate: single row */
        printf("%-8s ", sd->label);
        for (m = 0; m < sd->nmetrics; m++) {
            const metric_spec *ms = &sd->metrics[m];
            printf("%*.1f ", ms->col_width, vals[m]);
        }
        putchar('\n');
    } else {
        /* per-instance: one row per instance */
        for (i = 0; i < ninst; i++) {
            printf("%-8s %-12s ", sd->label,
                   insts ? insts[i] : "");
            for (m = 0; m < sd->nmetrics; m++) {
                const metric_spec *ms = &sd->metrics[m];
                printf("%*.1f ", ms->col_width,
                       vals[m * MAX_INSTS + i]);
            }
            putchar('\n');
        }
    }
}
