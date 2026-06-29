/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-colmux display — multi-line sorted and columnar output.
 * Uses VT100/ANSI escape codes and <termios.h> directly; no ncurses.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/ioctl.h>
#include "pcp-colmux.h"
#include "pcp-collectl.h"

/* ANSI escape helpers */
#define ESC_CLEAR   "\033[2J\033[H"
#define ESC_BOLD    "\033[1m"
#define ESC_RESET   "\033[0m"

static unsigned int
terminal_height(void)
{
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 4)
        return ws.ws_row;
    return 24;
}

/*
 * Print one row of multi-line sorted display.
 * Bold the sort column value.
 */
static void
print_sorted_row(const colmux_ctx *ctx, const host_state *h,
                 int col, int use_esc)
{
    unsigned int c;
    unsigned int ncols = ctx->ncols ? ctx->ncols : 8;
    unsigned int cw    = ctx->col_width;

    /* hostname */
    printf("%-*s ", (int)ctx->host_width, h->hostname);

    for (c = 0; c < ncols; c++) {
        double v = h->vals ? h->vals[c] : ctx->nodata_val;
        if (use_esc && (int)c == ctx->sort_col)
            printf("%s%*g%s ", ESC_BOLD, (int)cw, v, ESC_RESET);
        else
            printf("%*g ", (int)cw, v);
    }
    putchar('\n');
    (void)col;
}

void
display_header(colmux_ctx *ctx)
{
    unsigned int c;
    unsigned int ncols = ctx->ncols ? ctx->ncols : 8;

    printf("%-*s ", (int)ctx->host_width, "Host");
    for (c = 0; c < ncols; c++)
        printf("%*d ", (int)ctx->col_width, (int)c);
    putchar('\n');
}

/*
 * Multi-line sorted display — one row per host, sorted by sort_col.
 */
void
display_sorted(colmux_ctx *ctx)
{
    unsigned int i;
    unsigned int use_esc = !ctx->no_escape;
    unsigned int maxlines;
    unsigned int shown = 0;

    /* sort index array by sort_col value */
    unsigned int order[COLMUX_MAX_HOSTS];
    for (i = 0; i < ctx->nhosts; i++)
        order[i] = i;

    if (!ctx->freeze && ctx->nhosts > 1) {
        /* simple insertion sort on the sort column */
        unsigned int j;
        for (i = 1; i < ctx->nhosts; i++) {
            unsigned int key = order[i];
            double kv = ctx->hosts[key].vals ?
                        ctx->hosts[key].vals[ctx->sort_col] :
                        ctx->nodata_val;
            j = i;
            while (j > 0) {
                double pv = ctx->hosts[order[j-1]].vals ?
                            ctx->hosts[order[j-1]].vals[ctx->sort_col] :
                            ctx->nodata_val;
                int above = ctx->reverse ? (pv < kv) : (pv > kv);
                if (!above) break;
                order[j] = order[j-1];
                j--;
            }
            order[j] = key;
        }
    }

    maxlines = ctx->lines ? ctx->lines : terminal_height() - 3;

    if (use_esc)
        fputs(ESC_CLEAR, stdout);

    display_header(ctx);

    for (i = 0; i < ctx->nhosts && shown < maxlines; i++) {
        host_state *h = &ctx->hosts[order[i]];

        if (!h->avail)
            continue;
        if (ctx->zero_filter && h->vals && h->vals[ctx->sort_col] == 0.0)
            continue;

        print_sorted_row(ctx, h, ctx->sort_col, use_esc);
        shown++;
    }

    fflush(stdout);
}

/*
 * Columnar display (-cols): all hosts on one line per column selection.
 */
void
display_columnar(colmux_ctx *ctx)
{
    unsigned int i, c;
    unsigned int cw = ctx->col_width;

    /* header: column numbers */
    printf("%-*s", (int)ctx->host_width, "");
    for (c = 0; c < ctx->ncols; c++)
        printf(" %*d", (int)cw, ctx->cols[c]);
    putchar('\n');

    for (i = 0; i < ctx->nhosts; i++) {
        host_state *h = &ctx->hosts[i];
        double v;

        if (!h->avail)
            continue;

        printf("%-*s", (int)ctx->host_width, h->hostname);
        for (c = 0; c < ctx->ncols; c++) {
            int colidx = ctx->cols[c];
            v = (h->vals && colidx >= 0) ? h->vals[colidx] : ctx->nodata_val;

            if (ctx->col_div_1000)      v /= 1000.0;
            else if (ctx->col_div_k)    v /= 1024.0;
            if (ctx->col_log10 && v > 0) v = log10(v);

            printf(" %*g", (int)cw, v);
        }
        putchar('\n');
    }

    /* optional totals row */
    if (ctx->col_total) {
        printf("%-*s", (int)ctx->host_width, "TOTAL");
        for (c = 0; c < ctx->ncols; c++) {
            int colidx = ctx->cols[c];
            double total = 0.0;
            for (i = 0; i < ctx->nhosts; i++) {
                host_state *h = &ctx->hosts[i];
                if (h->avail && h->vals && colidx >= 0)
                    total += h->vals[colidx];
            }
            printf(" %*g", (int)cw, total);
        }
        putchar('\n');
    }

    fflush(stdout);
}
