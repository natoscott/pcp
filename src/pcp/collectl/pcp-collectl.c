/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * pcp-collectl - PCP-native reimplementation of the collectl monitoring tool.
 * Original collectl by Mark Seger (mjseger@gmail.com), HP.
 * See http://collectl.sourceforge.net/ for the original project.
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
#include "pcp-collectl.h"

#define DEFAULT_INTERVAL        1   /* seconds, interactive */
#define DEFAULT_INTERVAL_DAEMON 10  /* seconds, daemon mode */
#define DEFAULT_INTERVAL2       60  /* process/slab */
#define DEFAULT_INTERVAL3       120 /* environmental */
#define DEFAULT_HEADER_REPEAT   25  /* lines between header reprints */

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Collectl options"),
    /* collectl original options — must not be changed */
    { "count",     1, 'c', "N",       "collect N samples then exit" },
    { "interval",  1, 'i', "int[:int2[:int3]]", "collection intervals" },
    { "filename",  1, 'f', "FILE",    "output directory or file" },
    { "config",    1, 'C', "FILE",    "alternate config file" },
    { "daemon",    0, 'D', 0,         "run as background daemon" },
    { "plot",      0, 'P', 0,         "generate plot-format output" },
    { "playback",  1, 'p', "FILE",    "playback from collectl raw file" },
    { "subsys",    1, 's', "CODES",   "subsystems to monitor (e.g. cdn)" },
    { "verbose",   0,  0,  0,         "verbose (multi-line) output" },
    { "options",   1, 'o', "OPTS",    "output formatting options" },
    { "wide",      0, 'w', 0,         "wide output (no K/M/G suffixes)" },
    { "hr",        1,  0,  "N",       "repeat header every N lines" },
    { "address",   1, 'A', "ADDR[:PORT[:TIMEOUT]]", "socket/server mode" },
    { "rolllogs",  1, 'r', "TIME,DAYS,MINS", "log rotation spec" },
    { "flush",     1, 'F', "SECS",    "buffer flush interval" },
    { "nice",      0, 'N', 0,         "raise process priority" },
    { "align",     0,  0,  0,         "align samples to clock boundaries" },
    { "nohup",     0,  0,  0,         "don't exit if parent dies" },
    { "runas",     1,  0,  "UID[:GID]", "drop privileges after daemonising" },
    { "dskfilt",   1,  0,  "REGEX",   "output filter for disk instances" },
    { "netfilt",   1,  0,  "REGEX",   "output filter for network instances" },
    { "cpufilt",   1,  0,  "REGEX",   "output filter for CPU instances" },
    { "intfilt",   1,  0,  "REGEX",   "output filter for interrupt instances" },
    { "procfilt",  1,  0,  "REGEX",   "output filter for process instances" },
    { "top",       1,  0,  "[TYPE][,N]", "show top N processes by TYPE" },
    { "version",   0, 'v', 0,         "show version and exit" },
    /* new PCP-compatible aliases (no conflicts with above) */
    { "archive",   1, 'a', "FILE",    "alias for --playback" },
    PMAPI_OPTIONS_HEADER("Standard PCP options"),
    /*
     * -S/--start and -T/--finish are standard PCP time-window options;
     * neither letter is used by collectl so pmGetOptions handles them
     * automatically — no override needed.
     *
     * -D is overridden: collectl uses it for --daemon; PCP uses it for
     * --debug.  By including 'D' in short_options our switch case takes
     * ownership of -D (daemon), while --debug remains available as a
     * long option only via PMOPT_DEBUG below.
     */
    PMOPT_START,
    PMOPT_FINISH,
    PMOPT_DEBUG,    /* --debug long form only; -D overridden above */
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .version        = PMAPI_VERSION,
    /*
     * 'D' here overrides PMOPT_DEBUG's short form (-D) so that -D means
     * daemon mode for collectl compatibility.  All other short options are
     * unique to collectl and do not conflict with standard PCP options.
     */
    .short_options  = "c:i:f:C:DPp:s:o:wA:r:F:NvVa:",
    .long_options   = longopts,
    .short_usage    = "[options]",
    .flags          = PM_OPTFLAG_BOUNDARIES,
};

static void
parse_subsys(collectl_ctx *ctx, const char *spec)
{
    const char *p;
    unsigned int add = 1;

    for (p = spec; *p; p++) {
        if (*p == '+') { add = 1; continue; }
        if (*p == '-') { add = 0; continue; }
        switch (*p) {
        case 'c': case 'C':
		if (add) ctx->subsys |= (SS_CPU|SS_CPU_DETAIL);
                else     ctx->subsys &= ~(SS_CPU|SS_CPU_DETAIL);
		break;
        case 'd': if (add) ctx->subsys |= SS_DISK;    else ctx->subsys &= ~SS_DISK;    break;
        case 'D': if (add) ctx->subsys |= SS_DISK_DETAIL; else ctx->subsys &= ~SS_DISK_DETAIL; break;
        case 'm': if (add) ctx->subsys |= SS_MEMORY;  else ctx->subsys &= ~SS_MEMORY;  break;
        case 'M': if (add) ctx->subsys |= SS_NUMA;    else ctx->subsys &= ~SS_NUMA;    break;
        case 'n': if (add) ctx->subsys |= SS_NET;     else ctx->subsys &= ~SS_NET;     break;
        case 'N': if (add) ctx->subsys |= SS_NET_DETAIL; else ctx->subsys &= ~SS_NET_DETAIL; break;
        case 's': if (add) ctx->subsys |= SS_SOCK;    else ctx->subsys &= ~SS_SOCK;    break;
        case 't': if (add) ctx->subsys |= SS_TCP;     else ctx->subsys &= ~SS_TCP;     break;
        case 'T': if (add) ctx->subsys |= SS_TCP_DETAIL; else ctx->subsys &= ~SS_TCP_DETAIL; break;
        case 'i': if (add) ctx->subsys |= SS_INODE;   else ctx->subsys &= ~SS_INODE;   break;
        case 'j': if (add) ctx->subsys |= SS_IRQ;     else ctx->subsys &= ~SS_IRQ;     break;
        case 'J': if (add) ctx->subsys |= SS_IRQ_DETAIL; else ctx->subsys &= ~SS_IRQ_DETAIL; break;
        case 'y': if (add) ctx->subsys |= SS_SLAB;    else ctx->subsys &= ~SS_SLAB;    break;
        case 'Y': if (add) ctx->subsys |= SS_SLAB_DETAIL; else ctx->subsys &= ~SS_SLAB_DETAIL; break;
        case 'Z': if (add) ctx->subsys |= SS_PROCESS; else ctx->subsys &= ~SS_PROCESS; break;
        case 'b': if (add) ctx->subsys |= SS_BUDDY;   else ctx->subsys &= ~SS_BUDDY;   break;
        case 'f': if (add) ctx->subsys |= SS_NFS;     else ctx->subsys &= ~SS_NFS;     break;
        case 'F': if (add) ctx->subsys |= SS_NFS_DETAIL; else ctx->subsys &= ~SS_NFS_DETAIL; break;
        case 'E': if (add) ctx->subsys |= SS_ENVIRON; else ctx->subsys &= ~SS_ENVIRON; break;
        case 'l': if (add) ctx->subsys |= SS_LUSTRE;  else ctx->subsys &= ~SS_LUSTRE;  break;
        case 'x': if (add) ctx->subsys |= SS_IB;      else ctx->subsys &= ~SS_IB;      break;
        case 'X': if (add) ctx->subsys |= SS_IB_DETAIL; else ctx->subsys &= ~SS_IB_DETAIL; break;
        }
    }
}

static void
parse_intervals(collectl_ctx *ctx, const char *spec)
{
    unsigned long i1 = 0, i2 = 0, i3 = 0;
    if (sscanf(spec, "%lu:%lu:%lu", &i1, &i2, &i3) >= 1) {
        if (i1 && i1 <= 86400) ctx->interval  = (unsigned int)i1;
        if (i2 && i2 <= 86400) ctx->interval2 = (unsigned int)i2;
        if (i3 && i3 <= 86400) ctx->interval3 = (unsigned int)i3;
    }
}

/*
 * Parse collectl.conf key=value pairs and apply defaults not already
 * set by command-line.  DaemonCommands is expanded into argc/argv when
 * running in daemon mode, matching original collectl behaviour.
 */
static void
read_config(collectl_ctx *ctx, int daemon_mode,
            int *subsys_set, int *interval_set)
{
    FILE *fp;
    char  line[512];
    char *eq, *key, *val;
    int   num = 0;

    if ((fp = fopen(ctx->config, "r")) == NULL)
        return;     /* config file is optional */

    while (fgets(line, sizeof(line), fp)) {
        num++;
        line[strcspn(line, "\n\r")] = '\0';

        /* skip blank lines and comments */
        if (line[0] == '\0' || line[0] == '#')
            continue;

        if ((eq = strchr(line, '=')) == NULL) {
            fprintf(stderr, "pcp-collectl: config line %d: no '=', ignored\n",
                    num);
            continue;
        }

        /* split on first '=' only (DaemonCommands value may contain '=') */
        *eq = '\0';
        key = line;
        val = eq + 1;

        /* trim leading/trailing spaces from key */
        while (*key == ' ' || *key == '\t') key++;
        {
            char *e = key + strlen(key) - 1;
            while (e > key && (*e == ' ' || *e == '\t')) *e-- = '\0';
        }
        /* trim leading spaces from val */
        while (*val == ' ' || *val == '\t') val++;

        if (strcmp(key, "Interval") == 0 && !*interval_set) {
            long v = strtol(val, NULL, 10);
            if (v > 0 && v <= 86400) ctx->interval = (unsigned int)v;
        }
        else if (strcmp(key, "Interval2") == 0 && !*interval_set) {
            long v = strtol(val, NULL, 10);
            if (v > 0 && v <= 86400) ctx->interval2 = (unsigned int)v;
        }
        else if (strcmp(key, "Interval3") == 0 && !*interval_set) {
            long v = strtol(val, NULL, 10);
            if (v > 0 && v <= 86400) ctx->interval3 = (unsigned int)v;
        }
        else if (strcmp(key, "SubsysCore") == 0 && !*subsys_set) {
            ctx->subsys = SS_INTERACTIVE_DEFAULT;
            parse_subsys(ctx, val);
            *subsys_set = 1;
        }
        else if (strcmp(key, "DaemonCommands") == 0 && daemon_mode) {
            /*
             * Expand DaemonCommands by re-parsing them as if they were
             * command-line options.  Only applies when running as daemon.
             * We do a simple word-split and call parse_subsys / parse_intervals
             * for the options we recognise.
             */
            char *tok, *rest = val;
            while ((tok = strtok_r(rest, " \t", &rest)) != NULL) {
                if (strcmp(tok, "-s") == 0 || strcmp(tok, "--subsys") == 0) {
                    char *arg = strtok_r(NULL, " \t", &rest);
                    if (arg && !*subsys_set) {
                        parse_subsys(ctx, arg);
                        *subsys_set = 1;
                    }
                } else if (strcmp(tok, "-i") == 0 || strcmp(tok, "--interval") == 0) {
                    char *arg = strtok_r(NULL, " \t", &rest);
                    if (arg && !*interval_set)
                        parse_intervals(ctx, arg);
                } else if (strcmp(tok, "-f") == 0 || strcmp(tok, "--filename") == 0) {
                    char *arg = strtok_r(NULL, " \t", &rest);
                    if (arg && !ctx->filename)
                        ctx->filename = strdup(arg);
                }
            }
        }
        else if (strcmp(key, "LogVolSize") == 0)
            pmstrncpy(ctx->logvolsize, sizeof(ctx->logvolsize), val);
        else if (strcmp(key, "DiskFilter") == 0)
            filter_compile(&ctx->dskfilt, val);
    }

    fclose(fp);
}

int
main(int argc, char *argv[])
{
    int c;
    collectl_ctx ctx;
    int subsys_set = 0;
    int interval_set = 0;
    char lpath[MAXPATHLEN];

    memset(&ctx, 0, sizeof(ctx));
    ctx.interval       = DEFAULT_INTERVAL;
    ctx.purge_days     = 7;
    ctx.purge_months   = 12;
    ctx.interval2      = DEFAULT_INTERVAL2;
    ctx.interval3      = DEFAULT_INTERVAL3;
    ctx.count          = -1;
    ctx.header_repeat  = DEFAULT_HEADER_REPEAT;
    ctx.subsys         = SS_INTERACTIVE_DEFAULT;
    ctx.config         = COLLECTL_SYSCONF_PATH "/collectl/collectl.conf";

    /* read config before option parsing so CLI overrides take effect */
    read_config(&ctx, 0, &subsys_set, &interval_set);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
        switch (c) {
        case 'c': {
            long n = strtol(opts.optarg, NULL, 10);
            ctx.count = (n > 0) ? (int)n : -1;
            break;
        }
        case 'i':
            parse_intervals(&ctx, opts.optarg);
            interval_set = 1;
            break;
        case 'f':
            ctx.filename = opts.optarg;
            break;
        case 'C':
            ctx.config = opts.optarg;
            break;
        case 'D':
            ctx.daemon = 1;
            /* re-read config to apply DaemonCommands */
            read_config(&ctx, 1, &subsys_set, &interval_set);
            if (!subsys_set)
                ctx.subsys = SS_CORE_DEFAULT;
            if (!interval_set)
                ctx.interval = DEFAULT_INTERVAL_DAEMON;
            break;
        case 'P':
            /* plot format: TODO */
            break;
        case 'a':   /* --archive: alias for --playback */
        case 'p':
            ctx.playback = opts.optarg;
            break;
        case 's':
            parse_subsys(&ctx, opts.optarg);
            subsys_set = 1;
            break;
        case 'o':
            pmstrncpy(ctx.opts, sizeof(ctx.opts), opts.optarg);
            break;
        case 'w':
            ctx.wide = 1;
            break;
        case 'A':
            ctx.address = opts.optarg;
            break;
        case 'r': {
            /* -r HH:MM[,purgeDays[,rollMins]] */
            char tmp[64];
            char *p1, *p2;
            pmstrncpy(tmp, sizeof(tmp), opts.optarg);
            p1 = strchr(tmp, ',');
            if (p1) {
                *p1++ = '\0';
                p2 = strchr(p1, ',');
                if (p2) {
                    *p2++ = '\0';
                    ctx.roll_mins = (unsigned int)strtol(p2, NULL, 10);
                }
                ctx.purge_days = (unsigned int)strtol(p1, NULL, 10);
            }
            pmstrncpy(ctx.roll_time, sizeof(ctx.roll_time), tmp);
            break;
        }
        case 'N':   /* --nice */
            nice(10);
            break;
        case 'v': case 'V':
                printf("pcp-collectl version " PCP_COLLECTL_VERSION "\n");
            printf("Based on collectl " PCP_COLLECTL_VERSION
                   " by Mark Seger (mjseger@gmail.com)\n");
            printf("Copyright 2003-2018 Hewlett-Packard Development Company, L.P.\n");
            printf("Copyright (c) 2026 Red Hat.\n");
            printf("http://collectl.sourceforge.net/\n");
            exit(0);
        case 0:
            /* long options with no short form */
            if (strcmp(opts.long_options[opts.index].long_opt, "verbose") == 0)
                ctx.verbose = 1;
            else if (strcmp(opts.long_options[opts.index].long_opt, "align") == 0)
                ctx.align = 1;
            else if (strcmp(opts.long_options[opts.index].long_opt, "dskfilt") == 0)
                filter_compile(&ctx.dskfilt, opts.optarg);
            else if (strcmp(opts.long_options[opts.index].long_opt, "netfilt") == 0)
                filter_compile(&ctx.netfilt, opts.optarg);
            else if (strcmp(opts.long_options[opts.index].long_opt, "cpufilt") == 0)
                filter_compile(&ctx.cpufilt, opts.optarg);
            else if (strcmp(opts.long_options[opts.index].long_opt, "intfilt") == 0)
                filter_compile(&ctx.intfilt, opts.optarg);
            else if (strcmp(opts.long_options[opts.index].long_opt, "procfilt") == 0)
                filter_compile(&ctx.procfilt, opts.optarg);
            else if (strcmp(opts.long_options[opts.index].long_opt, "hr") == 0) {
                long n = strtol(opts.optarg, NULL, 10);
                if (n > 0 && n <= 10000)
                    ctx.header_repeat = (unsigned int)n;
            }
            break;
        }
    }

    if (opts.errors) {
        pmUsageMessage(&opts);
        exit(1);
    }

    /* Connect to pmcd (or set up archive/local context via pmGetOptions) */
    if (opts.context == PM_CONTEXT_ARCHIVE && opts.archives) {
        ctx.playback = opts.archives[0];
    }

    if (ctx.daemon)
        daemonise(&ctx);

    setup_signals();

    if (ctx.playback)
        return playback_loop(&ctx);

    /*
     * Use PM_CONTEXT_LOCAL to load DSO PMDAs directly in-process,
     * same pattern as sysstat sadc/pcp_local_init().  Point at the
     * local.conf PMDA list and local.root PMNS so the right DSOs load.
     */
    pmsprintf(lpath, sizeof(lpath), "%s/local.conf",
              pmGetConfig("PCP_SYSCONF_DIR"));
    setenv("PCP_PMCDCONF_FILE", lpath, 0);
    pmsprintf(lpath, sizeof(lpath), "%s/pmns/local.root",
              pmGetConfig("PCP_VAR_DIR"));
    setenv("PMNS_DEFAULT", lpath, 0);
    ctx.ctx = pmNewContext(PM_CONTEXT_LOCAL, NULL);
    if (ctx.ctx < 0) {
        fprintf(stderr, "%s: cannot create local context: %s\n",
                pmGetProgname(), pmErrStr(ctx.ctx));
        exit(1);
    }

    if (subsys_lookup(&ctx) < 0) {
        fprintf(stderr, "%s: metric lookup failed\n", pmGetProgname());
        exit(1);
    }

    if (ctx.filename || ctx.daemon) {
        if (archive_open(&ctx) < 0)
            exit(1);
    }

    return collect_loop(&ctx);
}
