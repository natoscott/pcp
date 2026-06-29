/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-colmux PDSH-format host-range expansion.
 * Parses formats like: n[1-10], n[1,3,5-8], host1,host2, @filename.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pcp-colmux.h"

#define MAX_RANGE   4096

/*
 * Expand a single bracket expression: prefix[1-5,7,10-12]suffix
 * into host list entries.  Returns count added.
 */
static int
expand_bracket(const char *prefix, const char *bracket, const char *suffix,
               char hosts[][COLMUX_HOSTNAMELEN], int start, int maxhosts)
{
    char buf[COLMUX_HOSTNAMELEN];
    char *tok, *rest;
    int count = 0;

    pmstrncpy(buf, sizeof(buf), bracket);
    rest = buf;

    while ((tok = strtok_r(rest, ",", &rest)) != NULL) {
        char *dash = strchr(tok, '-');
        if (dash) {
            long lo, hi, n;
            *dash = '\0';
            lo = strtol(tok, NULL, 10);
            hi = strtol(dash + 1, NULL, 10);
            /* detect zero-padding from the low end */
            int pad = (int)(dash - tok);
            for (n = lo; n <= hi && start + count < maxhosts; n++, count++) {
                if (pad > 1 && tok[0] == '0')
                    pmsprintf(hosts[start + count], COLMUX_HOSTNAMELEN,
                             "%s%0*ld%s", prefix, pad, n, suffix);
                else
                    pmsprintf(hosts[start + count], COLMUX_HOSTNAMELEN,
                             "%s%ld%s", prefix, n, suffix);
            }
        } else {
            if (start + count < maxhosts) {
                pmsprintf(hosts[start + count], COLMUX_HOSTNAMELEN,
                         "%s%s%s", prefix, tok, suffix);
                count++;
            }
        }
    }
    return count;
}

/*
 * Expand a single host specification (may contain brackets).
 * Returns count added to hosts[].
 */
static int
expand_one(const char *spec, char hosts[][COLMUX_HOSTNAMELEN], int start,
           int maxhosts)
{
    const char *lb = strchr(spec, '[');
    const char *rb;
    char prefix[128], bracket[COLMUX_HOSTNAMELEN], suffix[128];

    if (!lb) {
        /* simple hostname */
        if (start < maxhosts) {
            pmstrncpy(hosts[start], COLMUX_HOSTNAMELEN, spec);
            return 1;
        }
        return 0;
    }

    rb = strchr(lb, ']');
    if (!rb) {
        if (start < maxhosts) {
            pmstrncpy(hosts[start], COLMUX_HOSTNAMELEN, spec);
            return 1;
        }
        return 0;
    }

    /* split: prefix[bracket]suffix */
    pmstrncpy(prefix, sizeof(prefix), spec);
    prefix[lb - spec] = '\0';
    pmstrncpy(bracket, sizeof(bracket), lb + 1);
    bracket[rb - lb - 1] = '\0';
    pmstrncpy(suffix, sizeof(suffix), rb + 1);

    return expand_bracket(prefix, bracket, suffix, hosts, start, maxhosts);
}

/*
 * pdsh_expand — expand a host specification into a flat list.
 *
 * Supported formats:
 *   hostname               single host
 *   host1,host2,host3      comma-separated list
 *   n[1-10]                range expansion
 *   n[1,3,5-8]             mixed range/list
 *   @filename              read hostnames from file (one per line)
 *
 * Returns number of hosts added.
 */
int
pdsh_expand(const char *spec, char hosts[][COLMUX_HOSTNAMELEN], int maxhosts)
{
    int total = 0;

    /* @filename: read hosts from file */
    if (spec[0] == '@') {
        FILE *fp = fopen(spec + 1, "r");
        char line[COLMUX_HOSTNAMELEN];
        if (!fp) {
            fprintf(stderr, "pcp-colmux: cannot open host file: %s\n", spec + 1);
            return 0;
        }
        while (fgets(line, sizeof(line), fp) && total < maxhosts) {
            line[strcspn(line, "\n\r#")] = '\0';
            if (line[0] == '\0')
                continue;
            total += expand_one(line, hosts, total, maxhosts);
        }
        fclose(fp);
        return total;
    }

    /* comma-separated: split first, then expand each token */
    {
        char buf[4096];

        pmstrncpy(buf, sizeof(buf), spec);

        /*
         * Must not split on commas INSIDE brackets, e.g. n[1,3,5].
         * Simple approach: if there's a '[' before the next ',', defer
         * splitting until after the matching ']'.
         */
        char *p = buf;
        char *seg_start = p;
        int depth = 0;

        while (*p && total < maxhosts) {
            if (*p == '[') depth++;
            else if (*p == ']') depth--;
            else if (*p == ',' && depth == 0) {
                *p = '\0';
                total += expand_one(seg_start, hosts, total, maxhosts);
                seg_start = p + 1;
            }
            p++;
        }
        /* last segment */
        if (*seg_start && total < maxhosts)
            total += expand_one(seg_start, hosts, total, maxhosts);
    }

    return total;
}
