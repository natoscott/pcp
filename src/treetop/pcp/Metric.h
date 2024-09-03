#ifndef HEADER_Metric
#define HEADER_Metric
/*
htop - Metric.h
(C) 2020-2021 htop dev team
(C) 2020-2021 Red Hat, Inc.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <ctype.h>
#include <stdbool.h>
#include <pcp/pmapi.h>
#include <sys/time.h>

/* use htop config.h values for these macros, not pcp values */
#undef PACKAGE_URL
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_BUGREPORT


typedef enum Metric_ {
   PCP_CONTROL_THREADS,         /* proc.control.perclient.threads */

   PCP_TARGET_METRIC,		/* treetop.server.target.metric */
   PCP_TARGET_TIMESTAMP,	/* treetop.server.target.timestamp */
   PCP_TARGET_VALUESET,		/* treetop.server.target.valueset */
   PCP_SAMPLING_COUNT,		/* treetop.server.sampling.count */
   PCP_SAMPLING_INTERVAL,	/* treetop.server.sampling.interval */
   PCP_SAMPLING_ELAPSED,	/* treetop.server.sampling.elapsed_time */
   PCP_TRAINING_COUNT,		/* treetop.server.training.count */
   PCP_TRAINING_INTERVAL,	/* treetop.server.training.interval */
   PCP_TRAINING_WINDOW,		/* treetop.server.training.window */
   PCP_TRAINING_BOOSTED,	/* treetop.server.training.boosted_rounds */
   PCP_TRAINING_ELAPSED,	/* treetop.server.training.elapsed_time */
   PCP_FEATURES_ANOMALIES,	/* treetop.server.features.anomalies */
   PCP_FEATURES_MISSING,	/* treetop.server.features.missing_values */
   PCP_FEATURES_MUTUALINFO,	/* treetop.server.features.mutual_information */
   PCP_FEATURES_VARIANCE,	/* treetop.server.features.variance */
   PCP_FEATURES_TOTAL,		/* treetop.server.features.total */
   PCP_MODEL_CONFIDENCE,	/* treetop.server.explaining.model.confidence */
   PCP_MODEL_FEATURES,		/* treetop.server.explaining.model.features */
   PCP_MODEL_IMPORTANCE,	/* treetop.server.explaining.model.importance */
   PCP_IMPORTANCE_TYPE,		/* treetop.server.explaining.model.importance_type */
   PCP_MODEL_MUTUALINFO,	/* treetop.server.explaining.model.mutual_information */
   PCP_MODEL_ELAPSED,		/* treetop.server.explaining.model.elapsed_time */
   PCP_SHAP_FEATURES,		/* treetop.server.explaining.shap.features */
   PCP_SHAP_VALUES,		/* treetop.server.explaining.shap.values */
   PCP_SHAP_MUTUALINFO,		/* treetop.server.explaining.shap.mutual_information */
   PCP_SHAP_ELAPSED,		/* treetop.server.explaining.shap.elapsed_time */
   PCP_OPTMAX_CHANGE,		/* treetop.server.optimising.maxima.change */
   PCP_OPTMAX_DIRECTION,	/* treetop.server.optimising.maxima.direction */
   PCP_OPTMAX_FEATURES,		/* treetop.server.optimising.maxima.features */
   PCP_OPTMIN_CHANGE,		/* treetop.server.optimising.minima.change */
   PCP_OPTMIN_DIRECTION,	/* treetop.server.optimising.minima.direction */
   PCP_OPTMIN_FEATURES,		/* treetop.server.optimising.minima.features */
   PCP_OPTIMA_ELAPSED,		/* treetop.server.optimising.elapsed_time */

   PCP_HINV_NCPU,               /* hinv.ncpu */
   PCP_HINV_CPUCLOCK,           /* hinv.cpu.clock */
   PCP_UNAME_SYSNAME,           /* kernel.uname.sysname */
   PCP_UNAME_RELEASE,           /* kernel.uname.release */
   PCP_UNAME_MACHINE,           /* kernel.uname.machine */
   PCP_UNAME_DISTRO,            /* kernel.uname.distro */
   PCP_LOAD_AVERAGE,            /* kernel.all.load */
   PCP_PID_MAX,                 /* kernel.all.pid_max */
   PCP_UPTIME,                  /* kernel.all.uptime */
   PCP_BOOTTIME,                /* kernel.all.boottime */
   PCP_CPU_USER,                /* kernel.all.cpu.user */
   PCP_CPU_NICE,                /* kernel.all.cpu.nice */
   PCP_CPU_SYSTEM,              /* kernel.all.cpu.sys */
   PCP_CPU_IDLE,                /* kernel.all.cpu.idle */
   PCP_CPU_IOWAIT,              /* kernel.all.cpu.wait.total */
   PCP_CPU_IRQ,                 /* kernel.all.cpu.intr */
   PCP_CPU_SOFTIRQ,             /* kernel.all.cpu.irq.soft */
   PCP_CPU_STEAL,               /* kernel.all.cpu.steal */
   PCP_CPU_GUEST,               /* kernel.all.cpu.guest */
   PCP_CPU_GUESTNICE,           /* kernel.all.cpu.guest_nice */
   PCP_PERCPU_USER,             /* kernel.percpu.cpu.user */
   PCP_PERCPU_NICE,             /* kernel.percpu.cpu.nice */
   PCP_PERCPU_SYSTEM,           /* kernel.percpu.cpu.sys */
   PCP_PERCPU_IDLE,             /* kernel.percpu.cpu.idle */
   PCP_PERCPU_IOWAIT,           /* kernel.percpu.cpu.wait.total */
   PCP_PERCPU_IRQ,              /* kernel.percpu.cpu.intr */
   PCP_PERCPU_SOFTIRQ,          /* kernel.percpu.cpu.irq.soft */
   PCP_PERCPU_STEAL,            /* kernel.percpu.cpu.steal */
   PCP_PERCPU_GUEST,            /* kernel.percpu.cpu.guest */
   PCP_PERCPU_GUESTNICE,        /* kernel.percpu.cpu.guest_nice */
   PCP_MEM_TOTAL,               /* mem.physmem */
   PCP_MEM_FREE,                /* mem.util.free */
   PCP_MEM_BUFFERS,             /* mem.util.bufmem */
   PCP_MEM_CACHED,              /* mem.util.cached */
   PCP_MEM_SHARED,              /* mem.util.shared */
   PCP_MEM_AVAILABLE,           /* mem.util.available */
   PCP_MEM_SRECLAIM,            /* mem.util.slabReclaimable */
   PCP_MEM_SWAPCACHED,          /* mem.util.swapCached */
   PCP_MEM_SWAPTOTAL,           /* mem.util.swapTotal */
   PCP_MEM_SWAPFREE,            /* mem.util.swapFree */
   PCP_DISK_READB,              /* disk.all.read_bytes */
   PCP_DISK_WRITEB,             /* disk.all.write_bytes */
   PCP_DISK_ACTIVE,             /* disk.all.avactive */
   PCP_NET_RECVB,               /* network.all.in.bytes */
   PCP_NET_SENDB,               /* network.all.out.bytes */
   PCP_NET_RECVP,               /* network.all.in.packets */
   PCP_NET_SENDP,               /* network.all.out.packets */
   PCP_PSI_CPUSOME,             /* kernel.all.pressure.cpu.some.avg */
   PCP_PSI_IOSOME,              /* kernel.all.pressure.io.some.avg */
   PCP_PSI_IOFULL,              /* kernel.all.pressure.io.full.avg */
   PCP_PSI_IRQFULL,             /* kernel.all.pressure.irq.full.avg */
   PCP_PSI_MEMSOME,             /* kernel.all.pressure.memory.some.avg */
   PCP_PSI_MEMFULL,             /* kernel.all.pressure.memory.full.avg */
   PCP_MEM_ZSWAP,               /* mem.util.zswap */
   PCP_MEM_ZSWAPPED,            /* mem.util.zswapped */
   PCP_VFS_FILES_COUNT,         /* vfs.files.count */
   PCP_VFS_FILES_MAX,           /* vfs.files.max */

   PCP_PROC_PID,                /* proc.psinfo.pid */
   PCP_PROC_PPID,               /* proc.psinfo.ppid */
   PCP_PROC_TGID,               /* proc.psinfo.tgid */
   PCP_PROC_PGRP,               /* proc.psinfo.pgrp */
   PCP_PROC_SESSION,            /* proc.psinfo.session */
   PCP_PROC_STATE,              /* proc.psinfo.sname */
   PCP_PROC_TTY,                /* proc.psinfo.tty */
   PCP_PROC_TTYPGRP,            /* proc.psinfo.tty_pgrp */
   PCP_PROC_MINFLT,             /* proc.psinfo.minflt */
   PCP_PROC_MAJFLT,             /* proc.psinfo.maj_flt */
   PCP_PROC_CMINFLT,            /* proc.psinfo.cmin_flt */
   PCP_PROC_CMAJFLT,            /* proc.psinfo.cmaj_flt */
   PCP_PROC_UTIME,              /* proc.psinfo.utime */
   PCP_PROC_STIME,              /* proc.psinfo.stime */
   PCP_PROC_CUTIME,             /* proc.psinfo.cutime */
   PCP_PROC_CSTIME,             /* proc.psinfo.cstime */
   PCP_PROC_PRIORITY,           /* proc.psinfo.priority */
   PCP_PROC_NICE,               /* proc.psinfo.nice */
   PCP_PROC_THREADS,            /* proc.psinfo.threads */
   PCP_PROC_STARTTIME,          /* proc.psinfo.start_time */
   PCP_PROC_PROCESSOR,          /* proc.psinfo.processor */
   PCP_PROC_CMD,                /* proc.psinfo.cmd */
   PCP_PROC_PSARGS,             /* proc.psinfo.psargs */
   PCP_PROC_CGROUPS,            /* proc.psinfo.cgroups */
   PCP_PROC_OOMSCORE,           /* proc.psinfo.oom_score */
   PCP_PROC_VCTXSW,             /* proc.psinfo.vctxsw */
   PCP_PROC_NVCTXSW,            /* proc.psinfo.nvctxsw */
   PCP_PROC_LABELS,             /* proc.psinfo.labels */
   PCP_PROC_ENVIRON,            /* proc.psinfo.environ */
   PCP_PROC_TTYNAME,            /* proc.psinfo.ttyname */
   PCP_PROC_EXE,                /* proc.psinfo.exe */
   PCP_PROC_CWD,                /* proc.psinfo.cwd */

   PCP_PROC_AUTOGROUP_ID,       /* proc.autogroup.id */
   PCP_PROC_AUTOGROUP_NICE,     /* proc.autogroup.nice */

   PCP_PROC_ID_UID,             /* proc.id.uid */
   PCP_PROC_ID_USER,            /* proc.id.uid_nm */

   PCP_PROC_IO_RCHAR,           /* proc.io.rchar */
   PCP_PROC_IO_WCHAR,           /* proc.io.wchar */
   PCP_PROC_IO_SYSCR,           /* proc.io.syscr */
   PCP_PROC_IO_SYSCW,           /* proc.io.syscw */
   PCP_PROC_IO_READB,           /* proc.io.read_bytes */
   PCP_PROC_IO_WRITEB,          /* proc.io.write_bytes */
   PCP_PROC_IO_CANCELLED,       /* proc.io.cancelled_write_bytes */

   PCP_PROC_MEM_SIZE,           /* proc.memory.size */
   PCP_PROC_MEM_RSS,            /* proc.memory.rss */
   PCP_PROC_MEM_SHARE,          /* proc.memory.share */
   PCP_PROC_MEM_TEXTRS,         /* proc.memory.textrss */
   PCP_PROC_MEM_LIBRS,          /* proc.memory.librss */
   PCP_PROC_MEM_DATRS,          /* proc.memory.datrss */
   PCP_PROC_MEM_DIRTY,          /* proc.memory.dirty */

   PCP_PROC_SMAPS_PSS,          /* proc.smaps.pss */
   PCP_PROC_SMAPS_SWAP,         /* proc.smaps.swap */
   PCP_PROC_SMAPS_SWAPPSS,      /* proc.smaps.swappss */

   PCP_METRIC_COUNT             /* total metric count */
} Metric;

void Metric_enable(Metric metric, bool enable);

bool Metric_enabled(Metric metric);

void Metric_enableThreads(void);

bool Metric_fetch(struct timeval* timestamp);

bool Metric_iterate(Metric metric, int* instp, int* offsetp);

pmAtomValue* Metric_values(Metric metric, pmAtomValue* atom, int count, int type);

const pmDesc* Metric_desc(Metric metric);

int Metric_type(Metric metric);

int Metric_instanceCount(Metric metric);

int Metric_instanceOffset(Metric metric, int inst);

pmAtomValue* Metric_instance(Metric metric, int inst, int offset, pmAtomValue* atom, int type);

void Metric_externalName(Metric metric, int inst, char** externalName);

int Metric_lookupText(const char* metric, char** desc);

#endif
