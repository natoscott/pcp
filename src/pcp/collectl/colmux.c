/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-colmux main entry point and option parsing.
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
#include <termios.h>
#include <sys/ioctl.h>
#include "pcp-colmux.h"

/* forward declaration */
int colmux_playback(colmux_ctx *ctx);

static struct termios orig_termios;
static int raw_mode_active;

static void
restore_terminal(void)
{
    if (raw_mode_active)
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void
enable_raw_mode(void)
{
    struct termios raw;

    if (!isatty(STDIN_FILENO))
        return;
    if (tcgetattr(STDIN_FILENO, &orig_termios) < 0)
        return;
    atexit(restore_terminal);

    raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;   /* non-blocking */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    /* make stdin non-blocking so we can poll for keypresses */
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    raw_mode_active = 1;
}

/*
 * Read and process a single keypress (non-blocking).
 * Returns 1 if the display loop should exit.
 */
static int
handle_key(colmux_ctx *ctx)
{
    unsigned char buf[4];
    ssize_t n;

    n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0)
        return 0;

    if (buf[0] == 'q' || buf[0] == 'Q' || buf[0] == 3 /* Ctrl-C */)
        return 1;

    if (buf[0] == 'r') { ctx->reverse    ^= 1; return 0; }
    if (buf[0] == 'z') { ctx->zero_filter ^= 1; return 0; }
    if (buf[0] == 'f') { ctx->freeze     ^= 1; return 0; }

    /* arrow keys: ESC [ A/B/C/D */
    if (n >= 3 && buf[0] == 033 && buf[1] == '[') {
        switch (buf[2]) {
        case 'C':   /* right: next column */
            ctx->sort_col++;
            break;
        case 'D':   /* left: prev column */
            if (ctx->sort_col > 0) ctx->sort_col--;
            break;
        case 'A':   /* up: ascending */
            ctx->reverse = 0;
            break;
        case 'B':   /* down: descending */
            ctx->reverse = 1;
            break;
        }
    }
    return 0;
}

/*
 * colmux uses single-dash long options (-address, -command, -cols, etc.).
 * PM_OPTFLAG_LONG_ONLY makes pmGetOptions accept single-dash long options
 * as well as double-dash, matching the original colmux Perl behaviour.
 */
static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Common switches"),
    { "address",    1, 0, "addr[,addr,...]", "hosts to monitor (PDSH format supported)" },
    { "command",    1, 0, "string",          "pcp-collectl options (e.g. \"-s cdn -i 10\")" },
    { "port",       1, 0, "N",               "socket port (default 2655)" },
    { "timeout",    1, 0, "N",               "connection timeout in seconds" },
    { "hostwidth",  1, 0, "N",               "minimum hostname column width (default 8)" },
    { "lines",      1, 0, "N",               "max display lines (default: terminal height)" },
    { "noescape",   0, 0, 0,                 "disable VT100 escape sequences" },
    { "version",    0, 'v', 0,               "show version and exit" },
    { "help",       0, '?', 0,               "show help and exit" },
    PMAPI_OPTIONS_HEADER("Playback mode"),
    { "delay",      1, 0, "secs",            "pause between playback intervals" },
    { "nosort",     0, 0, 0,                 "disable sorting (also disables escapes/bold)" },
    PMAPI_OPTIONS_HEADER("Multi-line format"),
    { "colhelp",    0, 0, 0,                 "display column numbers" },
    { "column",     1, 0, "N",               "sort by column number (0-based)" },
    { "finalcr",    0, 0, 0,                 "print final carriage return" },
    { "hostformat", 1, 0, "char:piece",      "display piece of hostname" },
    { "nobold",     0, 0, 0,                 "disable column highlighting" },
    { "reverse",    0, 0, 0,                 "sort descending" },
    { "zero",       0, 0, 0,                 "exclude rows with zeros in sort column" },
    PMAPI_OPTIONS_HEADER("Single-line format"),
    { "col1000",    0, 0, 0,                 "divide columns by 1000" },
    { "colk",       0, 0, 0,                 "divide columns by 1024" },
    { "collog10",   0, 0, 0,                 "apply log10 to columns" },
    { "cols",       1, 0, "N[,N,...]",       "select specific columns to display" },
    { "colnodet",   0, 0, 0,                 "display totals only, not per-host" },
    { "colnodiv",   1, 0, "cols",            "don't divide specified columns" },
    { "colnoinst",  0, 0, 0,                 "don't include instance names in totals" },
    { "coltotal",   0, 0, 0,                 "print column totals to the right" },
    { "colwidth",   1, 0, "N",               "column width (default 6)" },
    PMAPI_OPTIONS_HEADER("Exception reporting"),
    { "age",        1, 0, "N",               "stale data threshold in intervals (default 2)" },
    { "negdataval", 1, 0, "val",             "value for negative numbers" },
    { "nodataval",  1, 0, "val",             "value for missing/stale data (default -1)" },
    PMAPI_OPTIONS_HEADER("Diagnostics"),
    { "debug",      1, 0, "N",               "debug bit mask" },
    { "nocheck",    0, 0, 0,                 "skip host reachability checks" },
    { "quiet",      0, 0, 0,                 "suppress warnings" },
    PMAPI_OPTIONS_HEADER("Miscellaneous"),
    { "colbin",     1, 0, "path",            "alternative path to collectl binary" },
    { "keepalive",  1, 0, "secs",            "SSH ServerAliveInterval" },
    { "retaddr",    1, 0, "addr",            "explicit return address for socket" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .version       = PMAPI_VERSION,
    .short_options = "v?",
    .long_options  = longopts,
    .short_usage   = "[-command \"pcp-collectl-options\"] "
                     "[-address addr[,addr,...]] [options]",
    /* PM_OPTFLAG_LONG_ONLY: accept single-dash long options as original colmux did */
    .flags         = PM_OPTFLAG_LONG_ONLY,
};

static void
parse_cols(colmux_ctx *ctx, const char *spec)
{
    char buf[256];
    char *tok, *rest;

    pmstrncpy(buf, sizeof(buf), spec);
    ctx->ncols = 0;
    rest = buf;
    while ((tok = strtok_r(rest, ",", &rest)) != NULL &&
           ctx->ncols < COLMUX_MAX_COLS) {
        long n = strtol(tok, NULL, 10);
        if (n >= 0)
            ctx->cols[ctx->ncols++] = (int)n;
    }
    ctx->cols_mode = 1;
}

static void
parse_addresses(colmux_ctx *ctx, const char *spec)
{
    char expanded[COLMUX_MAX_HOSTS][256];
    int n, i;

    n = pdsh_expand(spec, expanded, COLMUX_MAX_HOSTS);
    for (i = 0; i < n && ctx->nhosts < COLMUX_MAX_HOSTS; i++) {
        pmstrncpy(ctx->hosts[ctx->nhosts].hostname,
                  sizeof(ctx->hosts[ctx->nhosts].hostname),
                  expanded[i]);
        ctx->hosts[ctx->nhosts].ctx   = -1;
        ctx->hosts[ctx->nhosts].avail = 0;
        ctx->nhosts++;
    }
}

int
main(int argc, char *argv[])
{
    int c;
    colmux_ctx ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.sort_col   = 0;
    ctx.host_width = COLMUX_DEFAULT_HOSTW;
    ctx.col_width  = COLMUX_DEFAULT_COLW;
    ctx.age_limit  = COLMUX_DEFAULT_AGE;
    ctx.nodata_val = -1.0;
    ctx.port       = COLMUX_DEFAULT_PORT;
    ctx.timeout    = 10;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
        int idx = opts.index;
        const char *lopt = longopts[idx].long_opt;

        switch (c) {
        case 'v':
            printf("pcp-colmux version " PCP_COLLECTL_VERSION "\n");
            printf("Based on colmux by Mark Seger (mjseger@gmail.com)\n");
            printf("Copyright 2003-2018 Hewlett-Packard Development Company, L.P.\n");
            printf("Copyright (c) 2026 Red Hat.\n");
            printf("http://collectl-utils.sourceforge.net/colmux.html\n");
            exit(0);
        case 0:
            if (!lopt) break;
            if (strcmp(lopt, "address") == 0)
                parse_addresses(&ctx, opts.optarg);
            else if (strcmp(lopt, "command") == 0)
                pmstrncpy(ctx.command, sizeof(ctx.command), opts.optarg);
            else if (strcmp(lopt, "port") == 0)
                ctx.port = (int)strtol(opts.optarg, NULL, 10);
            else if (strcmp(lopt, "timeout") == 0)
                ctx.timeout = (int)strtol(opts.optarg, NULL, 10);
            else if (strcmp(lopt, "hostwidth") == 0) {
                long v = strtol(opts.optarg, NULL, 10);
                if (v > 0) ctx.host_width = (unsigned int)v;
            }
            else if (strcmp(lopt, "lines") == 0) {
                long v = strtol(opts.optarg, NULL, 10);
                if (v >= 0) ctx.lines = (unsigned int)v;
            }
            else if (strcmp(lopt, "noescape") == 0 ||
                     strcmp(lopt, "nosort") == 0)
                ctx.no_escape = 1;
            else if (strcmp(lopt, "nobold") == 0)
                ctx.no_bold = 1;
            else if (strcmp(lopt, "column") == 0) {
                long v = strtol(opts.optarg, NULL, 10);
                if (v >= 0) ctx.sort_col = (int)v;
            }
            else if (strcmp(lopt, "reverse") == 0)
                ctx.reverse = 1;
            else if (strcmp(lopt, "zero") == 0)
                ctx.zero_filter = 1;
            else if (strcmp(lopt, "cols") == 0)
                parse_cols(&ctx, opts.optarg);
            else if (strcmp(lopt, "col1000") == 0)
                ctx.col_div_1000 = 1;
            else if (strcmp(lopt, "colk") == 0)
                ctx.col_div_k = 1;
            else if (strcmp(lopt, "collog10") == 0)
                ctx.col_log10 = 1;
            else if (strcmp(lopt, "coltotal") == 0)
                ctx.col_total = 1;
            else if (strcmp(lopt, "colwidth") == 0) {
                long v = strtol(opts.optarg, NULL, 10);
                if (v > 0) ctx.col_width = (unsigned int)v;
            }
            else if (strcmp(lopt, "age") == 0) {
                long v = strtol(opts.optarg, NULL, 10);
                if (v > 0) ctx.age_limit = (unsigned int)v;
            }
            else if (strcmp(lopt, "nodataval") == 0)
                ctx.nodata_val = strtod(opts.optarg, NULL);
            else if (strcmp(lopt, "negdataval") == 0)
                ctx.negdata_val = strtod(opts.optarg, NULL);
            else if (strcmp(lopt, "delay") == 0)
                ctx.delay = strtod(opts.optarg, NULL);
            break;
        }
    }

    if (opts.errors || ctx.nhosts == 0) {
        pmUsageMessage(&opts);
        exit(opts.errors ? 1 : 0);
    }

    /* playback mode: open archives, step through, display */
    if (ctx.playback[0] != '\0')
        return colmux_playback(&ctx);

    /* live mode: connect to all hosts */
    if (mux_connect_all(&ctx) < 0) {
        /* try legacy socket connections for any unreachable hosts */
        if (legacy_socket_connect(&ctx) <= 0) {
            fprintf(stderr, "pcp-colmux: no hosts available\n");
            exit(1);
        }
    }

    /* enable interactive terminal if stdout is a tty */
    if (!ctx.no_escape && isatty(STDIN_FILENO))
        enable_raw_mode();

    /* main display + input loop */
    for (;;) {
        if (mux_fetch_all(&ctx) < 0)
            break;

        if (ctx.cols_mode)
            display_columnar(&ctx);
        else
            display_sorted(&ctx);

        /* poll for keypresses during the delay period */
        {
            double waited = 0.0;
            double step   = 0.05;   /* 50ms slices */
            double total  = ctx.delay > 0 ? ctx.delay : 0.0;

            do {
                if (handle_key(&ctx))
                    goto done;
                if (total > 0) {
                    usleep((useconds_t)(step * 1e6));
                    waited += step;
                }
            } while (waited < total);
        }
    }

done:
    restore_terminal();
    mux_disconnect_all(&ctx);
    return 0;
}
