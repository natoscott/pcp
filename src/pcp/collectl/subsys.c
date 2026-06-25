/*
 * Copyright (c) 2026 Red Hat.
 *
 * Generic subsystem fetch, rate-calculation, and output machinery for
 * pcp-collectl.  All subsystems share this code; per-subsystem details
 * (metric names, column headers, indoms) live in subsys_defs.c.
 *
 * Single pmFetch per interval across all active subsystems.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pcp-collectl.h"

#define MAX_TOTAL_METRICS   512     /* across all subsystems */
#define MAX_INSTS           4096    /* instances per subsystem */

/* Per-metric state within the combined fetch array */
typedef struct {
    unsigned int    ss_idx;     /* index into collectl_subsys[] */
    unsigned int    m_idx;      /* metric index within that subsystem */
    pmDesc          desc;
} metric_slot;

/* Instance cache entry — includes PMI archive handle */
typedef struct {
    int         inst;
    char        name[64];
    pmiHandle   handles[MAX_TOTAL_METRICS]; /* one per slot referencing this indom */
    unsigned int nhandles;
} inst_cache_entry;

/* Per-subsystem instance cache */
typedef struct {
    inst_cache_entry entries[MAX_INSTS];
    unsigned int     n;
} inst_cache;

/* Global state built during subsys_lookup() */
static pmID         all_pmids[MAX_TOTAL_METRICS];
static metric_slot  all_slots[MAX_TOTAL_METRICS];
static unsigned int n_all;

/* PMI archive handles for aggregate (PM_INDOM_NULL) metrics: [slot] */
static pmiHandle    agg_handles[MAX_TOTAL_METRICS];
static int          agg_handles_valid;  /* 1 once archive is open */

/* Previous sample values for rate calculation: [slot_idx][inst_idx] */
static double       prev_vals[MAX_TOTAL_METRICS][MAX_INSTS];
static int          prev_valid;     /* 1 after first fetch */

/* Per-subsystem instance name caches — sized at compile time */
#define MAX_SUBSYS  32  /* must be >= collectl_nsubsys */
static inst_cache   inst_caches[MAX_SUBSYS];

/* Extracted values for output: [slot_idx][inst_idx] */
static double       cur_vals[MAX_TOTAL_METRICS][MAX_INSTS];
static unsigned int cur_ninst[MAX_TOTAL_METRICS];

int
subsys_lookup(collectl_ctx *ctx)
{
    unsigned int s, m;

    n_all = 0;
    prev_valid = 0;
    memset(all_pmids, 0, sizeof(all_pmids));
    memset(inst_caches, 0, sizeof(inst_caches));

    for (s = 0; s < collectl_nsubsys; s++) {
        const subsys_def *sd = &collectl_subsys[s];
        const char *names[32];
        pmID        pmids[32];
        unsigned int nm;

        if (!(ctx->subsys & sd->ss_flag))
            continue;

        for (nm = 0; nm < sd->nmetrics && nm < 32; nm++)
            names[nm] = sd->metrics[nm].name;

        pmDesc descs[32];

        if (pmLookupName(sd->nmetrics, names, pmids) < 0) {
            /* partial success fine — pmids[i] == PM_ID_NULL on failure */
        }

        /* batch descriptor lookup for all successfully resolved pmids */
        if (pmLookupDescs(sd->nmetrics, pmids, descs) < 0) {
            /* partial success fine — descs[i].pmid == PM_ID_NULL on failure */
        }

        for (m = 0; m < sd->nmetrics; m++) {
            if (pmids[m] == PM_ID_NULL || descs[m].pmid == PM_ID_NULL)
                continue;
            if (n_all >= MAX_TOTAL_METRICS)
                break;
            all_pmids[n_all] = pmids[m];
            all_slots[n_all].ss_idx = s;
            all_slots[n_all].m_idx  = m;
            all_slots[n_all].desc   = descs[m];
            n_all++;
        }
    }

    return n_all > 0 ? 0 : -1;
}

/*
 * Single pmFetch for all active metrics, extract values, apply rate
 * calculation, store in cur_vals[][].  Called by output_interval().
 */
int
subsys_fetch_all(collectl_ctx *ctx)
{
    pmResult        *result;
    unsigned int     i, j;
    struct timespec  now;
    double           elapsed;

    if (n_all == 0)
        return 0;

    {
        int fetch_sts = pmFetch((int)n_all, all_pmids, &result);
        if (fetch_sts < 0)
            return fetch_sts;   /* propagate PM_ERR_EOL for archive playback */
    }

    clock_gettime(CLOCK_REALTIME, &now);
    elapsed = (now.tv_sec  - ctx->prev_ts.tv_sec) +
              (now.tv_nsec - ctx->prev_ts.tv_nsec) * 1e-9;
    if (elapsed <= 0.0)
        elapsed = (double)ctx->interval;

    /* reset ninst counts */
    memset(cur_ninst, 0, sizeof(cur_ninst));

    for (i = 0; i < (unsigned int)result->numpmid; i++) {
        pmValueSet     *vset = result->vset[i];
        unsigned int    slot;
        const subsys_def *sd;
        const metric_spec *ms;

        /* find matching slot by pmid */
        for (slot = 0; slot < n_all; slot++)
            if (all_pmids[slot] == vset->pmid)
                break;
        if (slot >= n_all || vset->numval <= 0)
            continue;

        sd = &collectl_subsys[all_slots[slot].ss_idx];
        ms = &sd->metrics[all_slots[slot].m_idx];

        for (j = 0; j < (unsigned int)vset->numval; j++) {
            pmAtomValue atom;
            double      raw;

            if (pmExtractValue(vset->valfmt, &vset->vlist[j],
                               all_slots[slot].desc.type,
                               &atom, PM_TYPE_DOUBLE) < 0)
                continue;

            raw = atom.d;

            /* rate calculation for counters */
            if ((ms->flags & MSF_RATE) && prev_valid)
                raw = (raw - prev_vals[slot][j]) / elapsed;
            else if (ms->flags & MSF_RATE)
                raw = 0.0;  /* first sample: no rate yet */

            prev_vals[slot][j] = atom.d;    /* save raw counter */

            /* scale */
            if (ms->scale != 0.0)
                raw *= ms->scale;

            cur_vals[slot][j] = raw;
            if (j + 1 > cur_ninst[slot])
                cur_ninst[slot] = j + 1;

            /* populate instance name cache for instanced metrics */
            if ((ms->flags & MSF_INSTANCED) && vset->numval > 1) {
                unsigned int si = all_slots[slot].ss_idx;
                inst_cache *ic;
                if (si >= MAX_SUBSYS)
                    continue;
                ic = &inst_caches[si];
                /* check if already cached */
                unsigned int k;
                for (k = 0; k < ic->n; k++)
                    if (ic->entries[k].inst == vset->vlist[j].inst)
                        break;
                if (k >= ic->n && ic->n < MAX_INSTS) {
                    char *iname = NULL;
                    if (pmNameInDom(sd->indom, vset->vlist[j].inst,
                                    &iname) == 0 && iname) {
                        unsigned int ne = ic->n;
                        ic->entries[ne].inst = vset->vlist[j].inst;
                        pmstrncpy(ic->entries[ne].name,
                                  sizeof(ic->entries[ne].name), iname);
                        ic->entries[ne].nhandles = 0;
                        free(iname);

                        /* allocate a PMI handle for each slot that uses this indom */
                        if (agg_handles_valid) {
                            unsigned int sl;
                            for (sl = 0; sl < n_all; sl++) {
                                if (collectl_subsys[all_slots[sl].ss_idx].indom
                                        == sd->indom) {
                                    const metric_spec *ms2 =
                                        &sd->metrics[all_slots[sl].m_idx];
                                    pmiHandle h2 = pmiGetHandle(ms2->name,
                                                       ic->entries[ne].name);
                                    ic->entries[ne].handles[sl] = h2;
                                }
                            }
                        }
                        ic->n++;
                    }
                }
            }
        }
    }

    pmFreeResult(result);
    ctx->prev_ts = now;
    prev_valid   = 1;
    return 0;
}

/*
 * Compatibility shim used by output.c — extracts already-fetched values
 * for one subsystem from the global cur_vals[][] table.
 */
int
subsys_fetch(collectl_ctx *ctx, const subsys_def *sd,
             double *vals, char **inst_names, unsigned int *ninst)
{
    unsigned int slot, j, si;
    unsigned int max_ninst = 0;

    (void)ctx;
    si = (unsigned int)(sd - collectl_subsys);

    for (slot = 0; slot < n_all; slot++) {
        if (all_slots[slot].ss_idx != si)
            continue;
        unsigned int m = all_slots[slot].m_idx;
        unsigned int n = cur_ninst[slot];
        for (j = 0; j < n; j++)
            vals[m * MAX_INSTS + j] = cur_vals[slot][j];
        if (n > max_ninst)
            max_ninst = n;
    }

    /* fill instance names from cache */
    if (inst_names && si < MAX_SUBSYS && inst_caches[si].n > 0) {
        inst_cache *ic = &inst_caches[si];
        for (j = 0; j < ic->n && j < max_ninst; j++)
            inst_names[j] = ic->entries[j].name;
    }

    *ninst = max_ninst ? max_ninst : 1;
    return (int)*ninst;
}

/*
 * Allocate PMI archive handles for all registered metrics.
 * Called from archive_open() after pmiAddMetric() for each metric.
 * Aggregate (PM_INDOM_NULL) handles are allocated immediately.
 * Per-instance handles are allocated lazily on first instance encounter
 * in subsys_fetch_all() — see inst_cache population below.
 */
void
subsys_alloc_handles(void)
{
    unsigned int    slot;
    const subsys_def  *sd;
    const metric_spec *ms;

    for (slot = 0; slot < n_all; slot++) {
        sd = &collectl_subsys[all_slots[slot].ss_idx];
        ms = &sd->metrics[all_slots[slot].m_idx];

        if (sd->indom == PM_INDOM_NULL) {
            agg_handles[slot] = pmiGetHandle(ms->name, "");
        } else {
            agg_handles[slot] = -1;  /* instanced: allocated lazily */
        }
    }
    agg_handles_valid = 1;
}

/*
 * Write all active subsystem metric values to the open PMI archive using
 * pre-allocated pmiHandle values (fast path — no string lookups per sample).
 * Called once per interval from archive_write() before pmiHighResWrite().
 */
void
subsys_archive_write(collectl_ctx *ctx)
{
    unsigned int      slot, j, si, ninst;
    pmAtomValue       atom;
    const subsys_def *sd;
    inst_cache       *ic;

    (void)ctx;

    if (!agg_handles_valid)
        return;

    for (slot = 0; slot < n_all; slot++) {
        si    = all_slots[slot].ss_idx;
        ninst = cur_ninst[slot];
        sd    = &collectl_subsys[si];
        ic    = (si < MAX_SUBSYS) ? &inst_caches[si] : NULL;

        if (ninst == 0)
            continue;

        for (j = 0; j < ninst; j++) {
            pmiHandle h;
            double v = cur_vals[slot][j];

            if (sd->indom == PM_INDOM_NULL) {
                h = agg_handles[slot];
            } else if (ic && j < ic->n) {
                /* use per-instance handle allocated in subsys_fetch_all() */
                h = ic->entries[j].handles[slot];
            } else {
                continue;
            }

            if (h < 0)
                continue;

            /* convert double back to native type for the atom */
            switch (all_slots[slot].desc.type) {
            case PM_TYPE_U32:
                atom.ul  = (unsigned int)(unsigned long long)v;  break;
            case PM_TYPE_32:
                atom.l   = (int)(long long)v;                    break;
            case PM_TYPE_U64:
                atom.ull = (unsigned long long)v;                break;
            case PM_TYPE_64:
                atom.ll  = (long long)v;                         break;
            case PM_TYPE_FLOAT:
                atom.f   = (float)v;                             break;
            default:
                atom.d   = v;                                    break;
            }
            pmiPutAtomValueHandle(h, &atom);
        }
    }
}

void
subsys_output_brief(const subsys_def *sd, double *vals,
                    char **insts, unsigned int ninst,
                    collectl_ctx *ctx)
{
    unsigned int m, si;

    si = (unsigned int)(sd - collectl_subsys);
    (void)insts;
    (void)ninst;
    (void)ctx;

    printf("%-8s ", sd->label);
    for (m = 0; m < sd->nmetrics; m++) {
        const metric_spec *ms = &sd->metrics[m];
        double v = vals[m * MAX_INSTS];
        if (ms->flags & MSF_PCT)
            printf("%*.1f ", ms->col_width, v);
        else
            printf("%*.0f ", ms->col_width, v);
    }
    putchar('\n');
    (void)si;
}

void
subsys_output_verbose(const subsys_def *sd, double *vals,
                      char **insts, unsigned int ninst,
                      collectl_ctx *ctx)
{
    unsigned int i, m;

    (void)ctx;

    if (sd->verbose_hdr)
        printf("%s\n", sd->verbose_hdr);

    if (sd->indom == PM_INDOM_NULL || ninst <= 1) {
        printf("%-8s ", sd->label);
        for (m = 0; m < sd->nmetrics; m++) {
            const metric_spec *ms = &sd->metrics[m];
            printf("%*.1f ", ms->col_width, vals[m * MAX_INSTS]);
        }
        putchar('\n');
    } else {
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

/* ------------------------------------------------------------------ */
/* colmux per-host fetch support                                        */
/* ------------------------------------------------------------------ */

unsigned int
subsys_n_all(void)
{
    return n_all;
}

/*
 * Initialise metric lookup for one remote host.  Uses the same all_pmids[]
 * built by subsys_lookup() — assumes metric sets are consistent across hosts
 * (same PCP version).  Switches to pcp_ctx to verify the pmids are valid.
 */
int
subsys_mux_lookup(int pcp_ctx)
{
    int saved = pmWhichContext();
    int sts   = 0;
    unsigned int slot;
    pmDesc desc;

    pmUseContext(pcp_ctx);
    for (slot = 0; slot < n_all; slot++) {
        if (pmLookupDesc(all_pmids[slot], &desc) < 0) {
            all_pmids[slot] = PM_ID_NULL;
            sts = -1;
        }
    }
    pmUseContext(saved);
    return sts;
}

/*
 * Fetch one interval for a single remote host into the caller-supplied
 * vals[] array (length n_all).  Rate calculation is done against
 * prev_vals[] using the elapsed time since prev_ts.
 * Context must be set to pcp_ctx by the caller before this call.
 */
int
subsys_mux_fetch(int pcp_ctx, double *vals, unsigned int nvals,
                 double *prev_vals, struct timespec *prev_ts)
{
    pmResult     *result;
    struct timespec now;
    double        elapsed;
    unsigned int  slot, j;
    int           saved;

    if (n_all == 0 || nvals < n_all)
        return -1;

    saved = pmWhichContext();
    pmUseContext(pcp_ctx);

    if (pmFetch((int)n_all, all_pmids, &result) < 0) {
        pmUseContext(saved);
        return -1;
    }

    clock_gettime(CLOCK_REALTIME, &now);
    elapsed = (now.tv_sec  - prev_ts->tv_sec) +
              (now.tv_nsec - prev_ts->tv_nsec) * 1e-9;
    if (elapsed <= 0.0)
        elapsed = 1.0;

    /* initialise to nodata */
    for (slot = 0; slot < n_all; slot++)
        vals[slot] = -1.0;

    for (j = 0; j < (unsigned int)result->numpmid; j++) {
        pmValueSet *vset = result->vset[j];
        unsigned int s;
        pmAtomValue atom;
        double raw;

        for (s = 0; s < n_all; s++)
            if (all_pmids[s] == vset->pmid)
                break;
        if (s >= n_all || vset->numval <= 0)
            continue;

        if (pmExtractValue(vset->valfmt, &vset->vlist[0],
                           all_slots[s].desc.type,
                           &atom, PM_TYPE_DOUBLE) < 0)
            continue;
        raw = atom.d;

        /* average over instances for instanced metrics */
        if (vset->numval > 1) {
            unsigned int k;
            double total = raw;
            for (k = 1; k < (unsigned int)vset->numval; k++) {
                if (pmExtractValue(vset->valfmt, &vset->vlist[k],
                                   all_slots[s].desc.type,
                                   &atom, PM_TYPE_DOUBLE) == 0)
                    total += atom.d;
            }
            raw = total / vset->numval;
        }

        if (all_slots[s].desc.sem == PM_SEM_COUNTER && prev_vals) {
            double prev = prev_vals[s];
            vals[s] = (prev >= 0.0) ? (raw - prev) / elapsed : 0.0;
            prev_vals[s] = raw;
        } else {
            vals[s] = raw;
            if (prev_vals)
                prev_vals[s] = raw;
        }

        /* apply scale factor from metric_spec */
        {
            const subsys_def  *sd = &collectl_subsys[all_slots[s].ss_idx];
            const metric_spec *ms = &sd->metrics[all_slots[s].m_idx];
            if (ms->scale != 0.0)
                vals[s] *= ms->scale;
        }
    }

    pmFreeResult(result);
    *prev_ts = now;
    pmUseContext(saved);
    return 0;
}
