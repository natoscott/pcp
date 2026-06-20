/*
 * Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2026 Red Hat.
 *
 * Subsystem metric definitions for pcp-collectl.
 * Each entry maps a collectl subsystem code to PCP metric names, column
 * headers, and output format.  All metric collection is via pmFetch —
 * no direct /proc reads.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "pcp-collectl.h"

/* ------------------------------------------------------------------ */
/* CPU subsystem (c/C)                                                  */
/* ------------------------------------------------------------------ */

static const metric_spec cpu_metrics[] = {
    { "kernel.all.cpu.user",       MSF_RATE|MSF_PCT, 0, "User%",  6 },
    { "kernel.all.cpu.nice",       MSF_RATE|MSF_PCT, 0, "Nice%",  6 },
    { "kernel.all.cpu.sys",        MSF_RATE|MSF_PCT, 0, "Sys%",   5 },
    { "kernel.all.cpu.wait.total", MSF_RATE|MSF_PCT, 0, "Wait%",  6 },
    { "kernel.all.cpu.irq.hard",   MSF_RATE|MSF_PCT, 0, "IRQ%",   5 },
    { "kernel.all.cpu.irq.soft",   MSF_RATE|MSF_PCT, 0, "Soft%",  5 },
    { "kernel.all.cpu.steal",      MSF_RATE|MSF_PCT, 0, "Steal%", 6 },
    { "kernel.all.cpu.guest",      MSF_RATE|MSF_PCT, 0, "Guest%", 6 },
    { "kernel.all.cpu.idle",       MSF_RATE|MSF_PCT, 0, "Idle%",  6 },
    /* busy% derived: 100 - idle% */
};

static const metric_spec cpu_detail_metrics[] = {
    { "kernel.percpu.cpu.user",       MSF_RATE|MSF_PCT|MSF_INSTANCED, 0, "User%",  6 },
    { "kernel.percpu.cpu.nice",       MSF_RATE|MSF_PCT|MSF_INSTANCED, 0, "Nice%",  6 },
    { "kernel.percpu.cpu.sys",        MSF_RATE|MSF_PCT|MSF_INSTANCED, 0, "Sys%",   5 },
    { "kernel.percpu.cpu.wait.total", MSF_RATE|MSF_PCT|MSF_INSTANCED, 0, "Wait%",  6 },
    { "kernel.percpu.cpu.irq.hard",   MSF_RATE|MSF_PCT|MSF_INSTANCED, 0, "IRQ%",   5 },
    { "kernel.percpu.cpu.irq.soft",   MSF_RATE|MSF_PCT|MSF_INSTANCED, 0, "Soft%",  5 },
    { "kernel.percpu.cpu.steal",      MSF_RATE|MSF_PCT|MSF_INSTANCED, 0, "Steal%", 6 },
    { "kernel.percpu.cpu.guest",      MSF_RATE|MSF_PCT|MSF_INSTANCED, 0, "Guest%", 6 },
    { "kernel.percpu.cpu.idle",       MSF_RATE|MSF_PCT|MSF_INSTANCED, 0, "Idle%",  6 },
    { "hinv.cpu.frequency_scaling.current", MSF_INSTANCED, 0, "MHz",  5 },
};

/* ------------------------------------------------------------------ */
/* Disk subsystem (d/D)                                                 */
/* ------------------------------------------------------------------ */

static const metric_spec disk_metrics[] = {
    { "disk.all.read_bytes",  MSF_RATE|MSF_KB, 1.0/1024, "KBRead",  7 },
    { "disk.all.write_bytes", MSF_RATE|MSF_KB, 1.0/1024, "KBWrit",  7 },
    { "disk.all.read",        MSF_RATE,         0,        "Reads",   5 },
    { "disk.all.write",       MSF_RATE,         0,        "Writes",  6 },
};

static const metric_spec disk_detail_metrics[] = {
    { "disk.dev.read_bytes",  MSF_RATE|MSF_KB|MSF_INSTANCED, 1.0/1024, "KBRead",  7 },
    { "disk.dev.write_bytes", MSF_RATE|MSF_KB|MSF_INSTANCED, 1.0/1024, "KBWrit",  7 },
    { "disk.dev.avactive",    MSF_RATE|MSF_PCT|MSF_INSTANCED, 0,        "Busy%",   5 },
    { "disk.dev.read",        MSF_RATE|MSF_INSTANCED,          0,        "Reads",   5 },
    { "disk.dev.write",       MSF_RATE|MSF_INSTANCED,          0,        "Writes",  6 },
    { "disk.dev.read_merge",  MSF_RATE|MSF_INSTANCED,          0,        "RdMrg",   5 },
    { "disk.dev.write_merge", MSF_RATE|MSF_INSTANCED,          0,        "WrMrg",   5 },
};

/* ------------------------------------------------------------------ */
/* Memory subsystem (m)                                                 */
/* ------------------------------------------------------------------ */

static const metric_spec mem_metrics[] = {
    { "mem.util.used",     0, 1.0/1024, "Used",    6 },
    { "mem.util.bufmem",   0, 1.0/1024, "Buff",    5 },
    { "mem.util.cached",   0, 1.0/1024, "Cach",    5 },
    { "mem.util.dirty",    0, 1.0/1024, "Dirty",   5 },
    { "mem.util.inactive", 0, 1.0/1024, "Inact",   5 },
    { "mem.util.slab",     0, 1.0/1024, "Slab",    5 },
    { "mem.util.mapped",   0, 1.0/1024, "Map",     4 },
};

/* ------------------------------------------------------------------ */
/* NUMA memory (M)                                                      */
/* ------------------------------------------------------------------ */

static const metric_spec numa_metrics[] = {
    { "mem.numa.util.total",    MSF_INSTANCED, 1.0/1024, "Total",  6 },
    { "mem.numa.util.used",     MSF_INSTANCED, 1.0/1024, "Used",   6 },
    { "mem.numa.util.free",     MSF_INSTANCED, 1.0/1024, "Free",   6 },
    { "mem.numa.util.active",   MSF_INSTANCED, 1.0/1024, "Active", 6 },
    { "mem.numa.util.inactive", MSF_INSTANCED, 1.0/1024, "Inact",  5 },
};

/* ------------------------------------------------------------------ */
/* Network subsystem (n/N)                                              */
/* ------------------------------------------------------------------ */

static const metric_spec net_metrics[] = {
    { "network.all.in.bytes",    MSF_RATE|MSF_KB, 1.0/1024, "RxKB",   6 },
    { "network.all.out.bytes",   MSF_RATE|MSF_KB, 1.0/1024, "TxKB",   6 },
    { "network.all.in.packets",  MSF_RATE,         0,        "RxPkt",  6 },
    { "network.all.out.packets", MSF_RATE,         0,        "TxPkt",  6 },
    { "network.all.in.errors",   MSF_RATE,         0,        "RxErr",  5 },
    { "network.all.out.errors",  MSF_RATE,         0,        "TxErr",  5 },
    { "network.all.in.drops",    MSF_RATE,         0,        "RxDrp",  5 },
    { "network.all.out.drops",   MSF_RATE,         0,        "TxDrp",  5 },
};

static const metric_spec net_detail_metrics[] = {
    { "network.interface.in.bytes",    MSF_RATE|MSF_KB|MSF_INSTANCED, 1.0/1024, "RxKB",   6 },
    { "network.interface.out.bytes",   MSF_RATE|MSF_KB|MSF_INSTANCED, 1.0/1024, "TxKB",   6 },
    { "network.interface.in.packets",  MSF_RATE|MSF_INSTANCED,         0,        "RxPkt",  6 },
    { "network.interface.out.packets", MSF_RATE|MSF_INSTANCED,         0,        "TxPkt",  6 },
    { "network.interface.in.errors",   MSF_RATE|MSF_INSTANCED,         0,        "RxErr",  5 },
    { "network.interface.out.errors",  MSF_RATE|MSF_INSTANCED,         0,        "TxErr",  5 },
    { "network.interface.in.drops",    MSF_RATE|MSF_INSTANCED,         0,        "RxDrp",  5 },
    { "network.interface.out.drops",   MSF_RATE|MSF_INSTANCED,         0,        "TxDrp",  5 },
};

/* ------------------------------------------------------------------ */
/* Socket counts (s)                                                    */
/* ------------------------------------------------------------------ */

static const metric_spec sock_metrics[] = {
    { "network.sockstat.total",        0, 0, "Tot",   4 },
    { "network.sockstat.tcp.inuse",    0, 0, "TCPIn", 5 },
    { "network.sockstat.tcp.orphan",   0, 0, "Orph",  4 },
    { "network.sockstat.tcp.tw",       0, 0, "TWait", 5 },
    { "network.sockstat.udp.inuse",    0, 0, "UDPIn", 5 },
    { "network.sockstat.raw.inuse",    0, 0, "Raw",   3 },
    { "network.sockstat.frag.inuse",   0, 0, "Frag",  4 },
};

/* ------------------------------------------------------------------ */
/* TCP summary (t)                                                      */
/* ------------------------------------------------------------------ */

static const metric_spec tcp_metrics[] = {
    { "network.tcp.activeopens",  MSF_RATE, 0, "ActOpn",  6 },
    { "network.tcp.passiveopens", MSF_RATE, 0, "PasOpn",  6 },
    { "network.tcp.currestab",    0,         0, "CurrEst", 7 },
    { "network.tcp.retranssegs",  MSF_RATE, 0, "Retrans", 7 },
    { "network.tcp.inerrs",       MSF_RATE, 0, "InErr",   6 },
    { "network.tcp.outrsts",      MSF_RATE, 0, "OutRst",  6 },
};

/* ------------------------------------------------------------------ */
/* Inode/file/dentry counts (i)                                         */
/* ------------------------------------------------------------------ */

static const metric_spec inode_metrics[] = {
    { "vfs.dentry.count", 0, 0, "Dentries", 8 },
    { "vfs.files.count",  0, 0, "Files",    5 },
    { "vfs.inodes.count", 0, 0, "Inodes",   6 },
};

/* ------------------------------------------------------------------ */
/* Interrupt summary (j)                                                */
/* ------------------------------------------------------------------ */

static const metric_spec irq_metrics[] = {
    { "kernel.all.intr", MSF_RATE, 0, "Intrpts", 7 },
};

static const metric_spec irq_detail_metrics[] = {
    { "kernel.percpu.interrupts", MSF_RATE|MSF_INSTANCED, 0, "Intr", 5 },
};

/* ------------------------------------------------------------------ */
/* Slab summary (y)                                                     */
/* ------------------------------------------------------------------ */

static const metric_spec slab_metrics[] = {
    { "mem.slabinfo.objects.active", MSF_INSTANCED, 0, "ActObj", 6 },
    { "mem.slabinfo.objects.total",  MSF_INSTANCED, 0, "TotObj", 6 },
    { "mem.slabinfo.slabs.active",   MSF_INSTANCED, 0, "ActSlb", 6 },
    { "mem.slabinfo.slabs.total",    MSF_INSTANCED, 0, "TotSlb", 6 },
};

/* ------------------------------------------------------------------ */
/* Process subsystem (Z)                                                */
/* ------------------------------------------------------------------ */

static const metric_spec proc_metrics[] = {
    { "proc.psinfo.pid",    MSF_INSTANCED, 0, "PID",   6 },
    { "proc.psinfo.utime",  MSF_RATE|MSF_PCT|MSF_INSTANCED, 0, "CPU%",  5 },
    { "proc.memory.vmrss",  MSF_INSTANCED, 1.0/1024, "RssMB", 6 },
    { "proc.memory.vmsize", MSF_INSTANCED, 1.0/1024, "VszMB", 6 },
    { "proc.psinfo.threads",MSF_INSTANCED, 0, "Thr",   3 },
    { "proc.psinfo.cmd",    MSF_INSTANCED, 0, "Command", 8 },
};

/* ------------------------------------------------------------------ */
/* Buddy / fragmentation (b)                                            */
/* ------------------------------------------------------------------ */

static const metric_spec buddy_metrics[] = {
    { "mem.buddyinfo.total",  MSF_INSTANCED, 0, "TotKB", 6 },
    { "mem.buddyinfo.pages",  MSF_INSTANCED, 0, "Pages", 5 },
};

/* ------------------------------------------------------------------ */
/* NFS client (f)                                                       */
/* ------------------------------------------------------------------ */

static const metric_spec nfs_metrics[] = {
    { "nfs.client.calls",   MSF_RATE, 0, "Calls",  5 },
    { "nfs3.client.calls",  MSF_RATE, 0, "V3Call", 6 },
    { "nfs4.client.calls",  MSF_RATE, 0, "V4Call", 6 },
};

/* ------------------------------------------------------------------ */
/* Environmental (E) — via IPMI PMDA, optional                         */
/* ------------------------------------------------------------------ */

static const metric_spec env_metrics[] = {
    { "ipmi.sensor.value", MSF_INSTANCED, 0, "Value", 5 },
};

/* ================================================================== */
/* Master subsystem table                                               */
/* ================================================================== */

const subsys_def collectl_subsys[] = {
    {
        'c', 'C', "CPU", SS_CPU,
        cpu_metrics, sizeof(cpu_metrics)/sizeof(cpu_metrics[0]),
        PM_INDOM_NULL,
        "#Time       User%  Nice%   Sys%  Wait%   IRQ%  Soft%  Steal%  Guest%  Busy%",
        "#Time CPU  User%  Nice%   Sys%  Wait%   IRQ%  Soft%  Steal%  Guest%  Busy%   MHz",
        NULL /* compute: busy = 100 - idle, applied in subsys.c */
    },
    {
        'd', 'D', "DSK", SS_DISK,
        disk_metrics, sizeof(disk_metrics)/sizeof(disk_metrics[0]),
        PM_INDOM_NULL,
        "#Time       KBRead   KBWrit  Reads  Writes",
        "#Time Disk    KBRead   KBWrit  Reads  Writes  RdMrg  WrMrg  Busy%",
        NULL
    },
    {
        'm', 0, "MEM", SS_MEMORY,
        mem_metrics, sizeof(mem_metrics)/sizeof(mem_metrics[0]),
        PM_INDOM_NULL,
        "#Time          Used   Buff   Cach  Dirty  Inact   Slab    Map",
        NULL,
        NULL
    },
    {
        'M', 0, "NUMA", SS_NUMA,
        numa_metrics, sizeof(numa_metrics)/sizeof(numa_metrics[0]),
        (pmInDom)0xf000013, /* 60.19 */
        "#Time Node   Total    Used    Free  Active  Inact",
        NULL,
        NULL
    },
    {
        'n', 'N', "NET", SS_NET,
        net_metrics, sizeof(net_metrics)/sizeof(net_metrics[0]),
        PM_INDOM_NULL,
        "#Time       RxKB   TxKB  RxPkt  TxPkt  RxErr  TxErr  RxDrp  TxDrp",
        "#Time Iface    RxKB   TxKB  RxPkt  TxPkt  RxErr  TxErr  RxDrp  TxDrp",
        NULL
    },
    {
        's', 0, "SOCK", SS_SOCK,
        sock_metrics, sizeof(sock_metrics)/sizeof(sock_metrics[0]),
        PM_INDOM_NULL,
        "#Time    Tot TCPIn  Orph TWait UDPIn  Raw Frag",
        NULL,
        NULL
    },
    {
        't', 0, "TCP", SS_TCP,
        tcp_metrics, sizeof(tcp_metrics)/sizeof(tcp_metrics[0]),
        PM_INDOM_NULL,
        "#Time  ActOpn PasOpn CurrEst Retrans  InErr OutRst",
        NULL,
        NULL
    },
    {
        'i', 0, "VFS", SS_INODE,
        inode_metrics, sizeof(inode_metrics)/sizeof(inode_metrics[0]),
        PM_INDOM_NULL,
        "#Time  Dentries  Files  Inodes",
        NULL,
        NULL
    },
    {
        'j', 'J', "INT", SS_IRQ,
        irq_metrics, sizeof(irq_metrics)/sizeof(irq_metrics[0]),
        PM_INDOM_NULL,
        "#Time    Intrpts",
        "#Time CPU   Intrpts",
        NULL
    },
    {
        'y', 'Y', "SLAB", SS_SLAB,
        slab_metrics, sizeof(slab_metrics)/sizeof(slab_metrics[0]),
        (pmInDom)0xf00000c, /* 60.12 */
        "#Time Slab  ActObj  TotObj  ActSlb  TotSlb",
        NULL,
        NULL
    },
    {
        'Z', 0, "PROC", SS_PROCESS,
        proc_metrics, sizeof(proc_metrics)/sizeof(proc_metrics[0]),
        (pmInDom)0xc00009, /* 3.9 */
        "#Time    PID  CPU%  RssMB  VszMB  Thr  Command",
        NULL,
        NULL
    },
    {
        'b', 0, "BUDDY", SS_BUDDY,
        buddy_metrics, sizeof(buddy_metrics)/sizeof(buddy_metrics[0]),
        (pmInDom)0xf00001f, /* 60.31 */
        "#Time Zone       TotKB  Pages",
        NULL,
        NULL
    },
    {
        'f', 0, "NFS", SS_NFS,
        nfs_metrics, sizeof(nfs_metrics)/sizeof(nfs_metrics[0]),
        PM_INDOM_NULL,
        "#Time  Calls V3Call V4Call",
        NULL,
        NULL
    },
    {
        'E', 0, "ENV", SS_ENVIRON,
        env_metrics, sizeof(env_metrics)/sizeof(env_metrics[0]),
        PM_INDOM_NULL,
        "#Time Sensor   Value",
        NULL,
        NULL
    },
};

const unsigned int collectl_nsubsys =
    sizeof(collectl_subsys) / sizeof(collectl_subsys[0]);
