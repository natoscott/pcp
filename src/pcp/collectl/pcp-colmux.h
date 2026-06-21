/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-colmux - multiplex monitoring data from multiple systems
 * running pcp-collectl.  Original colmux by Mark Seger (mjseger@gmail.com).
 * See http://collectl-utils.sourceforge.net/colmux.html
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef PCP_COLMUX_H
#define PCP_COLMUX_H

#include "pmapi.h"
#include <regex.h>

#define COLMUX_DEFAULT_PORT     2655
#define COLMUX_MAX_HOSTS        256
#define COLMUX_MAX_COLS         64
#define COLMUX_DEFAULT_HOSTW    8
#define COLMUX_DEFAULT_LINES    0       /* 0 = use terminal height */
#define COLMUX_DEFAULT_COLW     6
#define COLMUX_DEFAULT_AGE      2       /* intervals before stale */

/* ------------------------------------------------------------------ */
/* Per-host state                                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    char        hostname[256];
    char        username[64];   /* for SSH connections */
    int         ctx;            /* pmContext handle, -1 if unavailable */
    int         avail;          /* 1 if connected and responding */
    double     *vals;           /* last fetched values [ncols] */
    double     *prev_vals;      /* previous sample for rate calc */
    struct timespec prev_ts;
    unsigned int age;           /* intervals since last valid sample */
} host_state;

/* ------------------------------------------------------------------ */
/* Runtime context                                                      */
/* ------------------------------------------------------------------ */
typedef struct {
    /* host list (from -address) */
    host_state  hosts[COLMUX_MAX_HOSTS];
    unsigned int nhosts;

    /* collectl command string (from -command) */
    char        command[512];
    char        subsys[64];     /* parsed -s codes from command */

    /* display mode */
    int         cols_mode;      /* 1 = -cols columnar, 0 = multi-line sorted */
    int         cols[COLMUX_MAX_COLS];
    unsigned int ncols;
    int         sort_col;       /* column index to sort by */
    int         reverse;        /* sort descending */
    int         zero_filter;    /* skip all-zero rows */
    int         freeze;         /* don't auto-sort */
    int         no_escape;      /* disable VT100 escapes */
    int         no_bold;        /* disable bold highlighting */
    unsigned int host_width;    /* hostname column width */
    unsigned int lines;         /* max display lines */
    unsigned int col_width;     /* column width for -cols */

    /* playback */
    char        playback[512];  /* -p filespec */
    double      delay;          /* -delay seconds */

    /* time window */
    char        from_time[32];
    char        thru_time[32];

    /* legacy socket mode */
    int         port;
    int         timeout;

    /* scaling for -cols */
    int         col_div_1000;   /* -col1000 */
    int         col_div_k;      /* -colk */
    int         col_log10;      /* -collog10 */
    int         col_total;      /* -coltotal */

    /* stale data */
    unsigned int age_limit;     /* -age */
    double      nodata_val;     /* -nodataval (default -1) */
    double      negdata_val;    /* -negdataval */
} colmux_ctx;

/* ------------------------------------------------------------------ */
/* Function prototypes                                                  */
/* ------------------------------------------------------------------ */

/* pdsh.c */
int  pdsh_expand(const char *spec, char hosts[][256], int maxhosts);

/* mux.c */
int  mux_connect_all(colmux_ctx *ctx);
int  mux_fetch_all(colmux_ctx *ctx);
void mux_disconnect_all(colmux_ctx *ctx);

/* display.c */
void display_sorted(colmux_ctx *ctx);
void display_columnar(colmux_ctx *ctx);
void display_header(colmux_ctx *ctx);

/* colmux-socket.c */
int  legacy_socket_connect(colmux_ctx *ctx);
int  legacy_socket_read(colmux_ctx *ctx);

#endif /* PCP_COLMUX_H */
