/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-collectl - PCP-native C reimplementation of the Perl collectl utility.
 * Original collectl by Mark Seger (mjseger@gmail.com), HP.
 * See http://collectl.sourceforge.net/ for the original project.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#ifndef PCP_COLLECTL_H
#define PCP_COLLECTL_H

#include "pmapi.h"
#include "import.h"
#include <regex.h>

typedef int pmiHandle;  /* pmiGetHandle() return type alias for readability */

/* ------------------------------------------------------------------ */
/* Subsystem bitmask — one bit per collectl subsystem letter           */
/* ------------------------------------------------------------------ */
#define SS_CPU          (1U << 0)   /* c — CPU summary */
#define SS_CPU_DETAIL   (1U << 1)   /* C — per-CPU */
#define SS_DISK         (1U << 2)   /* d — disk summary */
#define SS_DISK_DETAIL  (1U << 3)   /* D — per-disk */
#define SS_MEMORY       (1U << 4)   /* m — memory summary */
#define SS_NUMA         (1U << 5)   /* M — NUMA node memory */
#define SS_NET          (1U << 6)   /* n — network summary */
#define SS_NET_DETAIL   (1U << 7)   /* N — per-interface */
#define SS_SOCK         (1U << 8)   /* s — socket counts */
#define SS_TCP          (1U << 9)   /* t — TCP summary */
#define SS_TCP_DETAIL   (1U << 10)  /* T — TCP detail */
#define SS_INODE        (1U << 11)  /* i — inodes/files/dentries */
#define SS_IRQ          (1U << 12)  /* j — interrupt summary */
#define SS_IRQ_DETAIL   (1U << 13)  /* J — per-CPU per-IRQ */
#define SS_SLAB         (1U << 14)  /* y — slab summary */
#define SS_SLAB_DETAIL  (1U << 15)  /* Y — per-slab detail */
#define SS_PROCESS      (1U << 16)  /* Z — processes */
#define SS_BUDDY        (1U << 17)  /* b — buddy/fragmentation info */
#define SS_NFS          (1U << 18)  /* f — NFS client summary */
#define SS_NFS_DETAIL   (1U << 19)  /* F — NFS detail */
#define SS_ENVIRON      (1U << 20)  /* E — environmental (IPMI) */
#define SS_LUSTRE       (1U << 21)  /* l — Lustre */
#define SS_IB           (1U << 22)  /* x — InfiniBand summary */
#define SS_IB_DETAIL    (1U << 23)  /* X — InfiniBand detail */

/* Collectl "core" default subsystem set for daemon mode */
#define SS_CORE_DEFAULT \
    (SS_BUDDY|SS_CPU|SS_DISK|SS_NFS|SS_INODE|SS_IRQ|SS_MEMORY| \
     SS_NET|SS_SOCK|SS_TCP)

/* Collectl "cdn" default for interactive mode */
#define SS_INTERACTIVE_DEFAULT  (SS_CPU|SS_DISK|SS_NET)

/* ------------------------------------------------------------------ */
/* Metric spec flags                                                    */
/* ------------------------------------------------------------------ */
#define MSF_RATE        (1U << 0)   /* counter — convert to rate/sec */
#define MSF_PCT         (1U << 1)   /* scale to percentage */
#define MSF_KB          (1U << 2)   /* scale bytes to KB */
#define MSF_INSTANCED   (1U << 3)   /* per-instance metric */

/* ------------------------------------------------------------------ */
/* Per-metric specification                                             */
/* ------------------------------------------------------------------ */
typedef struct {
    const char  *name;      /* PCP metric name */
    unsigned int flags;     /* MSF_* flags */
    double       scale;     /* multiply extracted value by this */
    const char  *col_hdr;  /* brief-mode column header */
    unsigned int col_width; /* column width for printf */
} metric_spec;

/* ------------------------------------------------------------------ */
/* Per-subsystem definition (all in subsys_defs.c)                     */
/* ------------------------------------------------------------------ */
typedef struct subsys_def {
    char             code;          /* primary code: 'c', 'd', 'm', ... */
    char             detail_code;   /* detail code: 'C', 'D', 'N', or 0 */
    const char      *label;         /* "CPU", "DSK", "MEM", "NET", ... */
    unsigned int     ss_flag;       /* SS_* bitmask bit for this subsystem */
    const metric_spec *metrics;
    unsigned int      nmetrics;
    pmInDom           indom;        /* PM_INDOM_NULL = aggregate */
    const char       *brief_hdr;   /* full brief-mode header line */
    const char       *verbose_hdr; /* verbose-mode header */
    /* optional: derived metric computation (e.g. busy%) */
    void (*compute)(double *vals, unsigned int nvals, double elapsed,
                    double *out, unsigned int *nout);
} subsys_def;

/* ------------------------------------------------------------------ */
/* Filter state (compiled regex per subsystem)                          */
/* ------------------------------------------------------------------ */
typedef struct {
    regex_t      rx;
    int          valid;     /* 1 if compiled */
    int          exclude;   /* 1 = exclude matching, 0 = include only */
} collectl_filter;

/* ------------------------------------------------------------------ */
/* Central runtime context                                              */
/* ------------------------------------------------------------------ */
typedef struct {
    int             ctx;            /* live pmContext handle */
    unsigned int    subsys;         /* active subsystem bitmask */
    unsigned int    interval;       /* primary interval in seconds */
    unsigned int    interval2;      /* process/slab interval (seconds) */
    unsigned int    interval3;      /* environmental interval (seconds) */
    int             count;          /* samples to collect, -1 = infinite */
    int             daemon;         /* -D flag */
    int             verbose;        /* --verbose flag */
    int             align;          /* --align flag */
    int             wide;           /* -w/--wide flag */
    unsigned int    header_repeat;  /* --hr N: repeat header every N lines */
    unsigned int    header_count;   /* lines since last header */

    /* Output paths */
    char           *filename;       /* -f output directory/path */
    char           *playback;       /* -p/-a archive path */
    char           *address;        /* -A socket address */
    char           *config;         /* -C config file path */

    /* Instance filters */
    collectl_filter cpufilt;
    collectl_filter dskfilt;
    collectl_filter netfilt;
    collectl_filter intfilt;
    collectl_filter procfilt;

    /* Rate calculation */
    struct timespec prev_ts;
    double        **prev_vals;      /* [nsubsys][max_metrics] */

    /* Output options string (from -o) */
    char            opts[64];       /* e.g. "dTz" */

    /* Log rotation (-r HH:MM[,purgeDays[,rollMins]]) */
    char            roll_time[8];   /* "HH:MM" */
    unsigned int    purge_days;     /* data file retention (default 7) */
    unsigned int    purge_months;   /* log file retention in months (default 12) */
    unsigned int    roll_mins;      /* sub-day interval in minutes (0 = daily) */
    time_t          next_roll;      /* epoch of next rotation */
} collectl_ctx;

/* ------------------------------------------------------------------ */
/* Global subsystem table (defined in subsys_defs.c)                   */
/* ------------------------------------------------------------------ */
extern const subsys_def collectl_subsys[];
extern const unsigned int collectl_nsubsys;

/* ------------------------------------------------------------------ */
/* Function prototypes                                                  */
/* ------------------------------------------------------------------ */

/* subsys.c — generic machinery */
int  subsys_lookup(collectl_ctx *ctx);
int  subsys_fetch_all(collectl_ctx *ctx);
void subsys_alloc_handles(void);
void subsys_archive_write(collectl_ctx *ctx);

/* subsys.c — colmux per-host fetch support */
extern unsigned int subsys_n_all(void);
int  subsys_mux_lookup(int pcp_ctx);
int  subsys_mux_fetch(int pcp_ctx, double *vals, unsigned int nvals,
                      double *prev_vals, struct timespec *prev_ts);
int  subsys_fetch(collectl_ctx *ctx, const subsys_def *sd,
                  double *vals, char **inst_names, unsigned int *ninst);
void subsys_output_brief(const subsys_def *sd, double *vals,
                         char **insts, unsigned int ninst,
                         collectl_ctx *ctx);
void subsys_output_verbose(const subsys_def *sd, double *vals,
                           char **insts, unsigned int ninst,
                           collectl_ctx *ctx);

/* collect.c */
int  collect_loop(collectl_ctx *ctx);

/* output.c */
void output_header(collectl_ctx *ctx);
void output_interval(collectl_ctx *ctx);

/* archive.c */
int  archive_open(collectl_ctx *ctx);
void archive_write(collectl_ctx *ctx);
void archive_close(collectl_ctx *ctx);

/* daemon.c */
int    daemonise(collectl_ctx *ctx);
void   setup_signals(void);
void   rotate_logs(collectl_ctx *ctx);
void   cull_old_archives(const char *dirpath, unsigned int purgeDays);
time_t next_roll_time(const char *rollTime, unsigned int rollMins);

/* filter.c */
int  filter_compile(collectl_filter *f, const char *pattern);
int  filter_match(collectl_filter *f, const char *name);
void filter_free(collectl_filter *f);

/* playback.c */
int  playback_loop(collectl_ctx *ctx);

/* socket.c */
int  socket_server(collectl_ctx *ctx);

/* daemon.c — signal flags referenced by collect.c, playback.c, socket.c */
extern volatile sig_atomic_t sigint_caught;
extern volatile sig_atomic_t sigusr1_caught;

/*
 * Config/pid paths — COLLECTL_SYSCONF_PATH and COLLECTL_RUN_PATH are
 * injected as -D compile flags by the GNUmakefile from builddefs variables.
 */
#define COLLECTL_PIDFILE    COLLECTL_RUN_PATH "/collectl.pid"

#endif /* PCP_COLLECTL_H */
