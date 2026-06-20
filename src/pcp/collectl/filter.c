/*
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-collectl instance name filtering.
 * Patterns prefixed with ^ invert the match (exclude matching instances).
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <string.h>
#include <stdio.h>
#include "pcp-collectl.h"

int
filter_compile(collectl_filter *f, const char *pattern)
{
    const char *p = pattern;
    int flags = REG_EXTENDED | REG_NOSUB;

    if (f->valid) {
        regfree(&f->rx);
        f->valid = 0;
    }
    f->exclude = 0;
    if (*p == '^') {
        f->exclude = 1;
        p++;
    }
    if (regcomp(&f->rx, p, flags) != 0) {
        fprintf(stderr, "pcp-collectl: invalid filter pattern: %s\n", pattern);
        return -1;
    }
    f->valid = 1;
    return 0;
}

/*
 * Returns 1 if the instance should be included in output, 0 to skip it.
 */
int
filter_match(collectl_filter *f, const char *name)
{
    int matched;

    if (!f->valid)
        return 1;   /* no filter — include everything */

    matched = (regexec(&f->rx, name, 0, NULL, 0) == 0);
    return f->exclude ? !matched : matched;
}

void
filter_free(collectl_filter *f)
{
    if (f->valid) {
        regfree(&f->rx);
        f->valid = 0;
    }
}
