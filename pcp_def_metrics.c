/*
 * pcp_def_metrics.c: Functions used by sadf to define PCP metrics
 * (C) 2019-2023 by Sebastien GODARD (sysstat <at> orange.fr)
 * (C) 2023 Red Hat.
 *
 ***************************************************************************
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published  by  the *
 * Free Software Foundation; either version 2 of the License, or (at  your *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it  will  be  useful,  but *
 * WITHOUT ANY WARRANTY; without the implied warranty  of  MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License *
 * for more details.                                                       *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA              *
 ***************************************************************************
 */

#include "common.h"
#include "sa.h"
#include "pcp_def_metrics.h"

#ifdef HAVE_PCP
#include <pcp/pmapi.h>
#include <pcp/import.h>
#ifdef HAVE_PCP_IMPL_H
#include <pcp/impl.h>
#endif

extern struct activity *act[];

/*
 ***************************************************************************
 * Insert PCP metrics definition metadata into an archive.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @metric	Metric ID (from pcp_common.h enumerations).
 ***************************************************************************
 */
void act_add_metric(struct activity *a, int metric)
{
	struct act_metrics *metrics = a->metrics;
	pmDesc *desc;
	char *name;

	if (!metrics || metrics->count > metric)
		PANIC(EINVAL);

	name = metrics->names[metric];
	desc = &metrics->descs[metric];
	pmiAddMetric(name, desc->pmid, desc->type, desc->indom, desc->sem, desc->units);
}

/*
 ***************************************************************************
 * Define PCP host metrics for an individual archive (file header).
 ***************************************************************************
 */
char *file_header_metric_names[] = {
	[FILE_HEADER_CPU_COUNT] = "hinv.ncpu",
	[FILE_HEADER_KERNEL_HERTZ] = "kernel.all.hz",
	[FILE_HEADER_UNAME_RELEASE] = "kernel.uname.release",
	[FILE_HEADER_UNAME_SYSNAME] = "kernel.uname.sysname",
	[FILE_HEADER_UNAME_MACHINE] = "kernel.uname.machine",
	[FILE_HEADER_UNAME_NODENAME] = "kernel.uname.nodename",
};

pmDesc file_header_metric_descs[] = {
	[FILE_HEADER_CPU_COUNT] = {
		.pmid = PMID_FILE_HEADER_CPU_COUNT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_KERNEL_HERTZ] = {
		.pmid = PMID_FILE_HEADER_KERNEL_HERTZ,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_UNAME_RELEASE] = {
		.pmid = PMID_FILE_HEADER_UNAME_RELEASE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_UNAME_SYSNAME] = {
		.pmid = PMID_FILE_HEADER_UNAME_SYSNAME,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_UNAME_MACHINE] = {
		.pmid = PMID_FILE_HEADER_UNAME_MACHINE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
	[FILE_HEADER_UNAME_NODENAME] = {
		.pmid = PMID_FILE_HEADER_UNAME_NODENAME,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
};

struct act_metrics file_header_metrics = {
	.count = FILE_HEADER_METRIC_COUNT,
	.names = file_header_metric_names,
	.descs = file_header_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP host metrics for an individual sample (record header).
 ***************************************************************************
 */
char *record_header_metric_names[] = {
	[RECORD_HEADER_KERNEL_UPTIME] = "kernel.all.uptime",
};

pmDesc record_header_metric_descs[] = {
	[RECORD_HEADER_KERNEL_UPTIME] = {
		.pmid = PMID_RECORD_HEADER_KERNEL_UPTIME,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_SEC, 0),
		.type = PM_TYPE_DOUBLE,
		.sem = PM_SEM_INSTANT,
	},
};

struct act_metrics record_header_metrics = {
	.count = RECORD_HEADER_METRIC_COUNT,
	.names = record_header_metric_names,
	.descs = record_header_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for per-CPU interrupts statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @cpu		CPU number (0 is cpu0, 1 is cpu1, etc.)
 ***************************************************************************
 */
void pcp_def_percpu_int_metrics(struct activity *a, int cpu)
{
	int inst = 0;
	char buf[64];
	struct sa_item *list;

	/* Create instance for each interrupt for the current CPU */
	for (list = a->item_list; list != NULL; list = list->next) {
		pmsprintf(buf, sizeof(buf), "%s::cpu%d", list->item_name, cpu);
		pmiAddInstance(PMI_INDOM(60, 40), buf, inst++);
	}

	act_add_metric(a, CPU_PERCPU_INTERRUPTS);
}

/*
 ***************************************************************************
 * Define PCP metrics for CPU related statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_cpu_metrics(struct activity *a)
{
	int i, first = TRUE;
	char buf[64];

	for (i = 0; (i < a->nr_ini) && (i < a->bitmap->b_size + 1); i++) {

		/*
		 * Should current CPU (including CPU "all") be displayed?
		 * NB: Offline not tested (they may be turned off and on within
		 * the same file.
		 */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
			/* CPU not selected */
			continue;

		if (!i) {
			if (a->id == A_CPU) {
				/* This is CPU "all" */
				act_add_metric(a, CPU_ALLCPU_USER);
				act_add_metric(a, CPU_ALLCPU_NICE);
				act_add_metric(a, CPU_ALLCPU_SYS);
				act_add_metric(a, CPU_ALLCPU_IDLE);
				act_add_metric(a, CPU_ALLCPU_WAITTOTAL);
				act_add_metric(a, CPU_ALLCPU_IRQTOTAL);
				act_add_metric(a, CPU_ALLCPU_IRQSOFT);
				act_add_metric(a, CPU_ALLCPU_IRQHARD);
				act_add_metric(a, CPU_ALLCPU_STEAL);
				act_add_metric(a, CPU_ALLCPU_GUEST);
				act_add_metric(a, CPU_ALLCPU_GUESTNICE);
			}

			else if (a->id == A_NET_SOFT) {
				/* Create metrics for A_NET_SOFT */
				act_add_metric(a, SOFTNET_ALLCPU_PROCESSED);
				act_add_metric(a, SOFTNET_ALLCPU_DROPPED);
				act_add_metric(a, SOFTNET_ALLCPU_TIMESQUEEZE);
				act_add_metric(a, SOFTNET_ALLCPU_RECEIVEDRPS);
				act_add_metric(a, SOFTNET_ALLCPU_FLOWLIMIT);
				act_add_metric(a, SOFTNET_ALLCPU_BACKLOGLENGTH);
			}
		}
		else {
			/* This is not CPU "all". */

			if (a->id == A_IRQ) {
				/* Create per-CPU interrupts metrics */
				pcp_def_percpu_int_metrics(a, i - 1);
			}

			else if (first) {
				/* Create instance for current CPU. */
				pmsprintf(buf, sizeof(buf), "cpu%d", i - 1);
				pmiAddInstance(PMI_INDOM(60, 0), buf, i - 1);

				if (a->id == A_CPU) {
					/* Create metrics for A_CPU */
					act_add_metric(a, CPU_PERCPU_USER);
					act_add_metric(a, CPU_PERCPU_NICE);
					act_add_metric(a, CPU_PERCPU_SYS);
					act_add_metric(a, CPU_PERCPU_IDLE);
					act_add_metric(a, CPU_PERCPU_WAITTOTAL);
					act_add_metric(a, CPU_PERCPU_IRQTOTAL);
					act_add_metric(a, CPU_PERCPU_IRQSOFT);
					act_add_metric(a, CPU_PERCPU_IRQHARD);
					act_add_metric(a, CPU_PERCPU_STEAL);
					act_add_metric(a, CPU_PERCPU_GUEST);
					act_add_metric(a, CPU_PERCPU_GUESTNICE);
				}

				else if (a->id == A_PWR_CPU) {
					/* Create metric for A_PWR_CPU */
					act_add_metric(a, POWER_PERCPU_CLOCK);
				}

				else if (a->id == A_NET_SOFT) {
					/* Create metrics for a_NET_SOFT */
					act_add_metric(a, SOFTNET_PERCPU_PROCESSED);
					act_add_metric(a, SOFTNET_PERCPU_DROPPED);
					act_add_metric(a, SOFTNET_PERCPU_TIMESQUEEZE);
					act_add_metric(a, SOFTNET_PERCPU_RECEIVEDRPS);
					act_add_metric(a, SOFTNET_PERCPU_FLOWLIMIT);
					act_add_metric(a, SOFTNET_PERCPU_BACKLOGLENGTH);
				}

				first = FALSE;
			}
		}
	}
}

char *cpu_metric_names[] = {
	[CPU_ALLCPU_USER] = "kernel.all.cpu.user",
	[CPU_ALLCPU_SYS] = "kernel.all.cpu.sys",
	[CPU_ALLCPU_NICE] = "kernel.all.cpu.nice",
	[CPU_ALLCPU_IDLE] = "kernel.all.cpu.idle",
	[CPU_ALLCPU_WAITTOTAL] = "kernel.all.cpu.wait.total",
	[CPU_ALLCPU_IRQTOTAL] = "kernel.all.intr",
	[CPU_ALLCPU_IRQSOFT] = "kernel.all.cpu.irq.soft",
	[CPU_ALLCPU_IRQHARD] = "kernel.all.cpu.irq.hard",
	[CPU_ALLCPU_GUEST] = "kernel.all.cpu.guest",
	[CPU_ALLCPU_GUESTNICE] = "kernel.all.cpu.guest_nice",
	[CPU_PERCPU_USER] = "kernel.percpu.cpu.user",
	[CPU_PERCPU_NICE] = "kernel.percpu.cpu.nice",
	[CPU_PERCPU_SYS] = "kernel.percpu.cpu.sys",
	[CPU_PERCPU_IDLE] = "kernel.percpu.cpu.idle",
	[CPU_PERCPU_WAITTOTAL] = "kernel.percpu.cpu.wait.total",
	[CPU_PERCPU_IRQTOTAL] = "kernel.percpu.intr",
	[CPU_PERCPU_IRQSOFT] = "kernel.percpu.cpu.irq.soft",
	[CPU_PERCPU_IRQHARD] = "kernel.percpu.cpu.irq.hard",
	[CPU_PERCPU_STEAL] = "kernel.percpu.cpu.steal",
	[CPU_PERCPU_GUEST] = "kernel.percpu.cpu.guest",
	[CPU_PERCPU_GUESTNICE] = "kernel.percpu.cpu.guest_nice",
	[CPU_PERCPU_INTERRUPTS] = "kernel.percpu.interrupts",
};

pmDesc cpu_metric_descs[] = {
	[CPU_ALLCPU_USER] = {
		.pmid = PMID_CPU_ALLCPU_USER,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_NICE] = {
		.pmid = PMID_CPU_ALLCPU_NICE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_SYS] = {
		.pmid = PMID_CPU_ALLCPU_SYS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_IDLE] = {
		.pmid = PMID_CPU_ALLCPU_IDLE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_WAITTOTAL] = {
		.pmid = PMID_CPU_ALLCPU_WAITTOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_IRQTOTAL] = {
		.pmid = PMID_CPU_ALLCPU_IRQTOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_IRQSOFT] = {
		.pmid = PMID_CPU_ALLCPU_IRQSOFT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_IRQHARD] = {
		.pmid = PMID_CPU_ALLCPU_IRQHARD,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_STEAL] = {
		.pmid = PMID_CPU_ALLCPU_STEAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_GUEST] = {
		.pmid = PMID_CPU_ALLCPU_GUEST,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_ALLCPU_GUESTNICE] = {
		.pmid = PMID_CPU_ALLCPU_GUESTNICE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_USER] = {
		.pmid = PMID_CPU_PERCPU_USER,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_NICE] = {
		.pmid = PMID_CPU_PERCPU_NICE,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_SYS] = {
		.pmid = PMID_CPU_PERCPU_SYS,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_IDLE] = {
		.pmid = PMID_CPU_PERCPU_IDLE,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_WAITTOTAL] = {
		.pmid = PMID_CPU_PERCPU_WAITTOTAL,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_IRQTOTAL] = {
		.pmid = PMID_CPU_PERCPU_IRQTOTAL,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_IRQSOFT] = {
		.pmid = PMID_CPU_PERCPU_IRQSOFT,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_IRQHARD] = {
		.pmid = PMID_CPU_PERCPU_IRQHARD,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_STEAL] = {
		.pmid = PMID_CPU_PERCPU_STEAL,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_GUEST] = {
		.pmid = PMID_CPU_PERCPU_GUEST,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_GUESTNICE] = {
		.pmid = PMID_CPU_PERCPU_GUESTNICE,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[CPU_PERCPU_INTERRUPTS] = {
		.pmid = PMID_CPU_PERCPU_INTERRUPTS,
		.indom = PMI_INDOM(60, 40),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics cpu_metrics = {
	.count = CPU_METRIC_COUNT,
	.names = cpu_metric_names,
	.descs = cpu_metric_descs,
};

char *softnet_metric_names[] = {
	[SOFTNET_ALLCPU_PROCESSED] = "network.softnet.processed",
	[SOFTNET_ALLCPU_DROPPED] = "network.softnet.dropped",
	[SOFTNET_ALLCPU_TIMESQUEEZE] = "network.softnet.time_squeeze",
	[SOFTNET_ALLCPU_RECEIVEDRPS] = "network.softnet.received_rps",
	[SOFTNET_ALLCPU_FLOWLIMIT] = "network.softnet.flow_limit",
	[SOFTNET_ALLCPU_BACKLOGLENGTH] = "network.softnet.backlog_length",
	[SOFTNET_PERCPU_PROCESSED] = "network.softnet.percpu.processed",
	[SOFTNET_PERCPU_DROPPED] = "network.softnet.percpu.dropped",
	[SOFTNET_PERCPU_TIMESQUEEZE] = "network.softnet.percpu.time_squeeze",
	[SOFTNET_PERCPU_RECEIVEDRPS] = "network.softnet.percpu.received_rps",
	[SOFTNET_PERCPU_FLOWLIMIT] = "network.softnet.percpu.flow_limit",
	[SOFTNET_PERCPU_BACKLOGLENGTH] = "network.softnet.percpu.backlog_length",
};

pmDesc softnet_metric_descs[] = {
	[SOFTNET_ALLCPU_PROCESSED] = {
		.pmid = PMID_SOFTNET_ALLCPU_PROCESSED,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_ALLCPU_DROPPED] = {
		.pmid = PMID_SOFTNET_ALLCPU_DROPPED,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_ALLCPU_TIMESQUEEZE] = {
		.pmid = PMID_SOFTNET_ALLCPU_TIMESQUEEZE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_ALLCPU_RECEIVEDRPS] = {
		.pmid = PMID_SOFTNET_ALLCPU_RECEIVEDRPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_ALLCPU_FLOWLIMIT] = {
		.pmid = PMID_SOFTNET_ALLCPU_FLOWLIMIT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_ALLCPU_BACKLOGLENGTH] = {
		.pmid = PMID_SOFTNET_ALLCPU_BACKLOGLENGTH,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_PERCPU_PROCESSED] = {
		.pmid = PMID_SOFTNET_PERCPU_PROCESSED,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_PERCPU_DROPPED] = {
		.pmid = PMID_SOFTNET_PERCPU_DROPPED,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_PERCPU_TIMESQUEEZE] = {
		.pmid = PMID_SOFTNET_PERCPU_TIMESQUEEZE,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_PERCPU_RECEIVEDRPS] = {
		.pmid = PMID_SOFTNET_PERCPU_RECEIVEDRPS,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_PERCPU_FLOWLIMIT] = {
		.pmid = PMID_SOFTNET_PERCPU_FLOWLIMIT,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[SOFTNET_PERCPU_BACKLOGLENGTH] = {
		.pmid = PMID_SOFTNET_PERCPU_BACKLOGLENGTH,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics softnet_metrics = {
	.count = SOFTNET_METRIC_COUNT,
	.names = softnet_metric_names,
	.descs = softnet_metric_descs,
};

char *power_cpu_metric_names[] = {
	[POWER_PERCPU_CLOCK] = "hinv.cpu.clock",
};

pmDesc power_cpu_metric_descs[] = {
	[POWER_PERCPU_CLOCK] = {
		.pmid = PMID_POWER_PERCPU_CLOCK,
		.indom = PMI_INDOM(60, 0),
		.units = PMI_UNITS(0, -1, 0, 0, PM_TIME_USEC, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_DISCRETE,
	},
};

struct act_metrics power_cpu_metrics = {
	.count = POWER_CPU_METRIC_COUNT,
	.names = power_cpu_metric_names,
	.descs = power_cpu_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for task creation and context switch statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pcsw_metrics(struct activity *a)
{
	act_add_metric(a, PCSW_CONTEXT_SWITCH);
	act_add_metric(a, PCSW_FORK_SYSCALLS);
}

char *pcsw_metric_names[] = {
	[PCSW_CONTEXT_SWITCH] = "kernel.all.pswitch",
	[PCSW_FORK_SYSCALLS] = "kernel.all.sysfork",
};

pmDesc pcsw_metric_descs[] = {
	[PCSW_CONTEXT_SWITCH] = {
		.pmid = PMID_PCSW_CONTEXT_SWITCH,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PCSW_FORK_SYSCALLS] = {
		.pmid = PMID_PCSW_FORK_SYSCALLS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics pcsw_metrics = {
	.count = PCSW_METRIC_COUNT,
	.names = pcsw_metric_names,
	.descs = pcsw_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for interrupts statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_irq_metrics(struct activity *a)
{
	int first = TRUE, inst = 0;
	struct sa_item *list;
	pmInDom indom;

	if (!(a->bitmap->b_array[0] & 1))
		/* CPU "all" not selected: Nothing to do here */
		return;

	/* Create domain */
	indom = PMI_INDOM(60, 4);

	/* Create instances and metrics for each interrupts for CPU "all" */
	for (list = a->item_list; list != NULL; list = list->next) {

		if (!strcmp(list->item_name, K_LOWERSUM)) {
			/*
			 * Create metric for interrupt "sum" for CPU "all".
			 * Interrupt "sum" appears at most once in list.
			 * No need to create an instance for it: It has a specific metric name.
			 */
			act_add_metric(a, IRQ_ALLIRQ_TOTAL);
		}
		else {
			if (first) {
				/* Create metric for a common interrupt for CPU "all" if not already done */
				act_add_metric(a, IRQ_PERIRQ_TOTAL);
				first = FALSE;
			}
			/* Create instance */
			pmiAddInstance(indom, list->item_name, inst++);
		}
	}
}

char *irq_metric_names[] = {
	[IRQ_ALLIRQ_TOTAL] = "kernel.all.intr",
	[IRQ_PERIRQ_TOTAL] = "kernel.all.interrupts.total",
};

pmDesc irq_metric_descs[] = {
	[IRQ_ALLIRQ_TOTAL] = {
		.pmid = PMID_IRQ_ALLIRQ_TOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[IRQ_PERIRQ_TOTAL] = {
		.pmid = PMID_IRQ_PERIRQ_TOTAL,
		.indom = PMI_INDOM(60, 4),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics irq_metrics = {
	.count = IRQ_METRIC_COUNT,
	.names = irq_metric_names,
	.descs = irq_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for swapping statistics.
 ***************************************************************************
 */
void pcp_def_swap_metrics(struct activity *a)
{
	act_add_metric(a, SWAP_PAGESIN);
	act_add_metric(a, SWAP_PAGESOUT);
}

char *swap_metric_names[] = {
	[SWAP_PAGESIN] = "swap.pagesin",
	[SWAP_PAGESOUT] = "swap.pagesout",
};

pmDesc swap_metric_descs[] = {
	[SWAP_PAGESIN] = {
		.pmid = PMID_SWAP_PAGESIN,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[SWAP_PAGESOUT] = {
		.pmid = PMID_SWAP_PAGESOUT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics swap_metrics = {
	.count = SWAP_METRIC_COUNT,
	.names = swap_metric_names,
	.descs = swap_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for paging statistics.
 ***************************************************************************
 */
void pcp_def_paging_metrics(struct activity *a)
{
	act_add_metric(a, PAGING_PGPGIN);
	act_add_metric(a, PAGING_PGPGOUT);
	act_add_metric(a, PAGING_PGFAULT);
	act_add_metric(a, PAGING_PGMAJFAULT);
	act_add_metric(a, PAGING_PGFREE);
	act_add_metric(a, PAGING_PGSCANDIRECT);
	act_add_metric(a, PAGING_PGSCANKSWAPD);
	act_add_metric(a, PAGING_PGSTEAL);
}

char *paging_metric_names[] = {
	[PAGING_PGPGIN] = "mem.vmstat.pgpgin",
	[PAGING_PGPGOUT] = "mem.vmstat.pgpgout",
	[PAGING_PGFAULT] = "mem.vmstat.pgfault",
	[PAGING_PGMAJFAULT] = "mem.vmstat.pgmajfault",
	[PAGING_PGFREE] = "mem.vmstat.pgfree",
	[PAGING_PGSCANDIRECT] = "mem.vmstat.pgscan_direct_total",
	[PAGING_PGSCANKSWAPD] = "mem.vmstat.pgscan_kswapd_total",
	[PAGING_PGSTEAL] = "mem.vmstat.pgsteal_total",
};

pmDesc paging_metric_descs[] = {
	[PAGING_PGPGIN] = {
		.pmid = PMID_PAGING_PGPGIN,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGPGOUT] = {
		.pmid = PMID_PAGING_PGPGOUT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGFAULT] = {
		.pmid = PMID_PAGING_PGFAULT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGMAJFAULT] = {
		.pmid = PMID_PAGING_PGMAJFAULT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGFREE] = {
		.pmid = PMID_PAGING_PGFREE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGSCANDIRECT] = {
		.pmid = PMID_PAGING_PGSCANDIRECT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGSCANKSWAPD] = {
		.pmid = PMID_PAGING_PGSCANKSWAPD,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PAGING_PGSTEAL] = {
		.pmid = PMID_PAGING_PGSTEAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics paging_metrics = {
	.count = PAGING_METRIC_COUNT,
	.names = paging_metric_names,
	.descs = paging_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for I/O and transfer rate statistics.
 ***************************************************************************
 */
void pcp_def_io_metrics(struct activity *a)
{
	act_add_metric(a, IO_ALLDEV_TOTAL);
	act_add_metric(a, IO_ALLDEV_READ);
	act_add_metric(a, IO_ALLDEV_WRITE);
	act_add_metric(a, IO_ALLDEV_DISCARD);
	act_add_metric(a, IO_ALLDEV_READBYTES);
	act_add_metric(a, IO_ALLDEV_WRITEBYTES);
	act_add_metric(a, IO_ALLDEV_DISCARDBYTES);
}

char *io_metric_names[] = {
	[IO_ALLDEV_TOTAL] = "disk.all.total",
	[IO_ALLDEV_READ] = "disk.all.read",
	[IO_ALLDEV_WRITE] = "disk.all.write",
	[IO_ALLDEV_DISCARD] = "disk.all.discard",
	[IO_ALLDEV_READBYTES] = "disk.all.read_bytes",
	[IO_ALLDEV_WRITEBYTES] = "disk.all.write_bytes",
	[IO_ALLDEV_DISCARDBYTES] = "disk.all.discard_bytes",
};

pmDesc io_metric_descs[] = {
	[IO_ALLDEV_TOTAL] = {
		.pmid = PMID_IO_ALLDEV_TOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[IO_ALLDEV_READ] = {
		.pmid = PMID_IO_ALLDEV_READ,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[IO_ALLDEV_WRITE] = {
		.pmid = PMID_IO_ALLDEV_WRITE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[IO_ALLDEV_DISCARD] = {
		.pmid = PMID_IO_ALLDEV_DISCARD,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[IO_ALLDEV_READBYTES] = {
		.pmid = PMID_IO_ALLDEV_READBYTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[IO_ALLDEV_WRITEBYTES] = {
		.pmid = PMID_IO_ALLDEV_WRITEBYTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[IO_ALLDEV_DISCARDBYTES] = {
		.pmid = PMID_IO_ALLDEV_DISCARDBYTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics io_metrics = {
	.count = IO_METRIC_COUNT,
	.names = io_metric_names,
	.descs = io_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for memory statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_memory_metrics(struct activity *a)
{
	if (DISPLAY_MEMORY(a->opt_flags)) {
		act_add_metric(a, MEM_PHYS_MB);
		act_add_metric(a, MEM_PHYS_KB);
		act_add_metric(a, MEM_UTIL_FREE);
		act_add_metric(a, MEM_UTIL_AVAIL);
		act_add_metric(a, MEM_UTIL_USED);
		act_add_metric(a, MEM_UTIL_BUFFER);
		act_add_metric(a, MEM_UTIL_CACHED);
		act_add_metric(a, MEM_UTIL_COMMITAS);
		act_add_metric(a, MEM_UTIL_ACTIVE);
		act_add_metric(a, MEM_UTIL_INACTIVE);
		act_add_metric(a, MEM_UTIL_DIRTY);

		if (DISPLAY_MEM_ALL(a->opt_flags)) {
			act_add_metric(a, MEM_UTIL_ANON);
			act_add_metric(a, MEM_UTIL_SLAB);
			act_add_metric(a, MEM_UTIL_KSTACK);
			act_add_metric(a, MEM_UTIL_PGTABLE);
			act_add_metric(a, MEM_UTIL_VMALLOC);
		}
	}

	if (DISPLAY_SWAP(a->opt_flags)) {
		act_add_metric(a, MEM_UTIL_SWAPFREE);
		act_add_metric(a, MEM_UTIL_SWAPTOTAL);
		act_add_metric(a, MEM_UTIL_SWAPCACHED);
	}
}

char *mem_metric_names[] = {
	[MEM_PHYS_MB] = "hinv.physmem",
	[MEM_PHYS_KB] = "mem.physmem",
	[MEM_UTIL_FREE] = "mem.util.free",
	[MEM_UTIL_AVAIL] = "mem.util.available",
	[MEM_UTIL_USED] = "mem.util.used",
	[MEM_UTIL_BUFFER] = "mem.util.bufmem",
	[MEM_UTIL_CACHED] = "mem.util.cached",
	[MEM_UTIL_COMMITAS] = "mem.util.committed_AS",
	[MEM_UTIL_ACTIVE] = "mem.util.active",
	[MEM_UTIL_INACTIVE] = "mem.util.inactive",
	[MEM_UTIL_DIRTY] = "mem.util.dirty",
	[MEM_UTIL_ANON] = "mem.util.anonpages",
	[MEM_UTIL_SLAB] = "mem.util.slab",
	[MEM_UTIL_KSTACK] = "mem.util.kernelStack",
	[MEM_UTIL_PGTABLE] = "mem.util.pageTables",
	[MEM_UTIL_VMALLOC] = "mem.util.vmallocUsed",
	[MEM_UTIL_SWAPFREE] = "mem.util.swapFree",
	[MEM_UTIL_SWAPTOTAL] = "mem.util.swapTotal",
	[MEM_UTIL_SWAPCACHED] = "mem.util.swapCached",
};

pmDesc mem_metric_descs[] = {
	[MEM_PHYS_MB] = {
		.pmid = PMID_MEM_PHYS_MB,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_MBYTE, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_DISCRETE,
	},
	[MEM_PHYS_KB] = {
		.pmid = PMID_MEM_PHYS_KB,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_DISCRETE,
	},
	[MEM_UTIL_FREE] = {
		.pmid = PMID_MEM_UTIL_FREE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_AVAIL] = {
		.pmid = PMID_MEM_UTIL_AVAIL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_USED] = {
		.pmid = PMID_MEM_UTIL_USED,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_BUFFER] = {
		.pmid = PMID_MEM_UTIL_BUFFER,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_CACHED] = {
		.pmid = PMID_MEM_UTIL_CACHED,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_COMMITAS] = {
		.pmid = PMID_MEM_UTIL_COMMITAS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_ACTIVE] = {
		.pmid = PMID_MEM_UTIL_ACTIVE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_INACTIVE] = {
		.pmid = PMID_MEM_UTIL_INACTIVE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_DIRTY] = {
		.pmid = PMID_MEM_UTIL_DIRTY,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_ANON] = {
		.pmid = PMID_MEM_UTIL_ANON,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_SLAB] = {
		.pmid = PMID_MEM_UTIL_SLAB,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_KSTACK] = {
		.pmid = PMID_MEM_UTIL_KSTACK,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_PGTABLE] = {
		.pmid = PMID_MEM_UTIL_PGTABLE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_VMALLOC] = {
		.pmid = PMID_MEM_UTIL_VMALLOC,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_SWAPFREE] = {
		.pmid = PMID_MEM_UTIL_SWAPFREE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_SWAPTOTAL] = {
		.pmid = PMID_MEM_UTIL_SWAPTOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_UTIL_SWAPCACHED] = {
		.pmid = PMID_MEM_UTIL_SWAPCACHED,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
};

struct act_metrics mem_metrics = {
	.count = MEM_METRIC_COUNT,
	.names = mem_metric_names,
	.descs = mem_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for kernel tables statistics.
 ***************************************************************************
 */
void pcp_def_ktables_metrics(struct activity *a)
{
	act_add_metric(a, KTABLE_DENTRYS);
	act_add_metric(a, KTABLE_FILES);
	act_add_metric(a, KTABLE_INODES);
	act_add_metric(a, KTABLE_PTYS);
}

char *ktable_metric_names[] = {
	[KTABLE_DENTRYS] = "vfs.dentry.count",
	[KTABLE_FILES] = "vfs.files.count",
	[KTABLE_INODES] = "vfs.inodes.count",
	[KTABLE_PTYS] = "kernel.all.nptys",
};

pmDesc ktable_metric_descs[] = {
	[KTABLE_DENTRYS] = {
		.pmid = PMID_KTABLE_DENTRYS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[KTABLE_FILES] = {
		.pmid = PMID_KTABLE_FILES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[KTABLE_INODES] = {
		.pmid = PMID_KTABLE_INODES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[KTABLE_PTYS] = {
		.pmid = PMID_KTABLE_PTYS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
};

struct act_metrics ktable_metrics = {
	.count = KTABLE_METRIC_COUNT,
	.names = ktable_metric_names,
	.descs = ktable_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for queue and load statistics.
 ***************************************************************************
 */
void pcp_def_queue_metrics(struct activity *a)
{
	pmInDom indom = PMI_INDOM(60, 2);

	pmiAddInstance(indom, "1 minute", 1);
	pmiAddInstance(indom, "5 minute", 5);
	pmiAddInstance(indom, "15 minute", 15);

	act_add_metric(a, KQUEUE_RUNNABLE);
	act_add_metric(a, KQUEUE_PROCESSES);
	act_add_metric(a, KQUEUE_BLOCKED);
	act_add_metric(a, KQUEUE_LOADAVG);
}

char *kqueue_metric_names[] = {
	[KQUEUE_RUNNABLE] = "kernel.all.runnable",
	[KQUEUE_PROCESSES] = "kernel.all.nprocs",
	[KQUEUE_BLOCKED] = "kernel.all.blocked",
	[KQUEUE_LOADAVG] = "kernel.all.load",
};

pmDesc kqueue_metric_descs[] = {
	[KQUEUE_RUNNABLE] = {
		.pmid = PMI_ID(60, 2, 2),
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[KQUEUE_PROCESSES] = {
		.pmid = PMI_ID(60, 2, 3),
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[KQUEUE_BLOCKED] = {
		.pmid = PMI_ID(60, 0, 16),
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[KQUEUE_LOADAVG] = {
		.pmid = PMI_ID(60, 2, 0),
		.indom = PMI_INDOM(60, 2),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
};

struct act_metrics kqueue_metrics = {
	.count = KQUEUE_METRIC_COUNT,
	.names = kqueue_metric_names,
	.descs = kqueue_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for disks statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_disk_metrics(struct activity *a)
{
	int inst = 0;
	static int setup;
	struct sa_item *list;

	if (!setup) {
		setup = 1;

		/* Create instances */
		for (list = a->item_list; list != NULL; list = list->next) {
			pmiAddInstance(PMI_INDOM(60, 1), list->item_name, inst++);
		}
	}

	act_add_metric(a, DISK_PERDEV_READ);
	act_add_metric(a, DISK_PERDEV_WRITE);
	act_add_metric(a, DISK_PERDEV_TOTAL);
	act_add_metric(a, DISK_PERDEV_TOTALBYTES);
	act_add_metric(a, DISK_PERDEV_READBYTES);
	act_add_metric(a, DISK_PERDEV_WRITEBYTES);
	act_add_metric(a, DISK_PERDEV_DISCARDBYTES);
	act_add_metric(a, DISK_PERDEV_READACTIVE);
	act_add_metric(a, DISK_PERDEV_WRITEACTIVE);
	act_add_metric(a, DISK_PERDEV_TOTALACTIVE);
	act_add_metric(a, DISK_PERDEV_DISCARDACTIVE);
	act_add_metric(a, DISK_PERDEV_AVACTIVE);
	act_add_metric(a, DISK_PERDEV_AVQUEUE);
}

char *disk_metric_names[] = {
	[DISK_PERDEV_READ] = "disk.dev.read",
	[DISK_PERDEV_WRITE] = "disk.dev.write",
	[DISK_PERDEV_TOTAL] = "disk.dev.total",
	[DISK_PERDEV_TOTALBYTES] = "disk.dev.total_bytes",
	[DISK_PERDEV_READBYTES] = "disk.dev.read_bytes",
	[DISK_PERDEV_WRITEBYTES] = "disk.dev.write_bytes",
	[DISK_PERDEV_DISCARDBYTES] = "disk.dev.discard_bytes",
	[DISK_PERDEV_READACTIVE] = "disk.dev.read_rawactive",
	[DISK_PERDEV_WRITEACTIVE] = "disk.dev.write_rawactive",
	[DISK_PERDEV_TOTALACTIVE] = "disk.dev.total_rawactive",
	[DISK_PERDEV_DISCARDACTIVE] = "disk.dev.discard_rawactive",
	[DISK_PERDEV_AVACTIVE] = "disk.dev.avactive",
	[DISK_PERDEV_AVQUEUE] = "disk.dev.aveq",
};

pmDesc disk_metric_descs[] = {
	[DISK_PERDEV_READ] = {
		.pmid = PMID_DISK_PERDEV_READ,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_WRITE] = {
		.pmid = PMID_DISK_PERDEV_WRITE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_TOTAL] = {
		.pmid = PMID_DISK_PERDEV_TOTAL,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_TOTALBYTES] = {
		.pmid = PMID_DISK_PERDEV_TOTALBYTES,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_READBYTES] = {
		.pmid = PMID_DISK_PERDEV_READBYTES,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_WRITEBYTES] = {
		.pmid = PMID_DISK_PERDEV_WRITEBYTES,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_DISCARDBYTES] = {
		.pmid = PMID_DISK_PERDEV_DISCARDBYTES,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_READACTIVE] = {
		.pmid = PMID_DISK_PERDEV_READACTIVE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_WRITEACTIVE] = {
		.pmid = PMID_DISK_PERDEV_WRITEACTIVE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_TOTALACTIVE] = {
		.pmid = PMID_DISK_PERDEV_TOTALACTIVE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_DISCARDACTIVE] = {
		.pmid = PMID_DISK_PERDEV_DISCARDACTIVE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_AVACTIVE] = {
		.pmid = PMID_DISK_PERDEV_AVACTIVE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[DISK_PERDEV_AVQUEUE] = {
		.pmid = PMID_DISK_PERDEV_AVQUEUE,
		.indom = PMI_INDOM(60, 1),
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_MSEC, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics disk_metrics = {
	.count = DISK_METRIC_COUNT,
	.names = disk_metric_names,
	.descs = disk_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for network interfaces (errors) statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_dev_metrics(struct activity *a)
{
	int inst = 0;
	static int setup;
	struct sa_item *list;

	if (!setup) {
		setup = 1;

		/* Create instances */
		for (list = a->item_list; list != NULL; list = list->next) {
			pmiAddInstance(PMI_INDOM(60, 3), list->item_name, inst++);
		}
	}

	if (a->id == A_NET_DEV) {
		/* Create metrics for A_NET_DEV */
		act_add_metric(a, NET_PERINTF_INPACKETS);
		act_add_metric(a, NET_PERINTF_OUTPACKETS);
		act_add_metric(a, NET_PERINTF_INBYTES);
		act_add_metric(a, NET_PERINTF_OUTBYTES);
		act_add_metric(a, NET_PERINTF_INCOMPRESS);
		act_add_metric(a, NET_PERINTF_OUTCOMPRESS);
		act_add_metric(a, NET_PERINTF_INMULTICAST);
	}
	else {
		/* Create metrics for A_NET_EDEV */
		act_add_metric(a, NET_EPERINTF_INERRORS);
		act_add_metric(a, NET_EPERINTF_OUTERRORS);
		act_add_metric(a, NET_EPERINTF_COLLISIONS);
		act_add_metric(a, NET_EPERINTF_INDROPS);
		act_add_metric(a, NET_EPERINTF_OUTDROPS);
		act_add_metric(a, NET_EPERINTF_OUTCARRIER);
		act_add_metric(a, NET_EPERINTF_INFRAME);
		act_add_metric(a, NET_EPERINTF_INFIFO);
		act_add_metric(a, NET_EPERINTF_OUTFIFO);
	}
}

char *netdev_metric_names[] = {
	[NET_PERINTF_INPACKETS] = "network.interface.in.packets",
	[NET_PERINTF_OUTPACKETS] = "network.interface.out.packets",
	[NET_PERINTF_INBYTES] = "network.interface.in.bytes",
	[NET_PERINTF_OUTBYTES] = "network.interface.out.bytes",
	[NET_PERINTF_INCOMPRESS] = "network.interface.in.compressed",
	[NET_PERINTF_OUTCOMPRESS] = "network.interface.out.compressed",
	[NET_PERINTF_INMULTICAST] = "network.interface.in.mcasts",
};

pmDesc netdev_metric_descs[] = {
	[NET_PERINTF_INPACKETS] = {
		.pmid = PMID_NET_PERINTF_INPACKETS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_PERINTF_OUTPACKETS] = {
		.pmid = PMID_NET_PERINTF_OUTPACKETS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_PERINTF_INBYTES] = {
		.pmid = PMID_NET_PERINTF_INBYTES,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_PERINTF_OUTBYTES] = {
		.pmid = PMID_NET_PERINTF_OUTBYTES,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_PERINTF_INCOMPRESS] = {
		.pmid = PMID_NET_PERINTF_INCOMPRESS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_PERINTF_OUTCOMPRESS] = {
		.pmid = PMID_NET_PERINTF_OUTCOMPRESS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_PERINTF_INMULTICAST] = {
		.pmid = PMID_NET_PERINTF_INMULTICAST,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics netdev_metrics = {
	.count = NET_PERINTF_METRIC_COUNT,
	.names = netdev_metric_names,
	.descs = netdev_metric_descs,
};

char *netedev_metric_names[] = {
	[NET_EPERINTF_INERRORS] = "network.interface.in.errors",
	[NET_EPERINTF_OUTERRORS] = "network.interface.out.errors",
	[NET_EPERINTF_COLLISIONS] = "network.interface.collisions",
	[NET_EPERINTF_INDROPS] = "network.interface.in.drops",
	[NET_EPERINTF_OUTDROPS] = "network.interface.out.drops",
	[NET_EPERINTF_OUTCARRIER] = "network.interface.out.carrier",
	[NET_EPERINTF_INFRAME] = "network.interface.in.frame",
	[NET_EPERINTF_INFIFO] = "network.interface.in.fifo",
	[NET_EPERINTF_OUTFIFO] = "network.interface.out.fifo",
};

pmDesc netedev_metric_descs[] = {
	[NET_EPERINTF_INERRORS] = {
		.pmid = PMID_NET_EPERINTF_INERRORS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_OUTERRORS] = {
		.pmid = PMID_NET_EPERINTF_OUTERRORS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_COLLISIONS] = {
		.pmid = PMID_NET_EPERINTF_COLLISIONS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_INDROPS] = {
		.pmid = PMID_NET_EPERINTF_INDROPS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_OUTDROPS] = {
		.pmid = PMID_NET_EPERINTF_OUTDROPS,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_OUTCARRIER] = {
		.pmid = PMID_NET_EPERINTF_OUTCARRIER,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_INFRAME] = {
		.pmid = PMID_NET_EPERINTF_INFRAME,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_INFIFO] = {
		.pmid = PMID_NET_EPERINTF_INFIFO,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EPERINTF_OUTFIFO] = {
		.pmid = PMID_NET_EPERINTF_OUTFIFO,
		.indom = PMI_INDOM(60, 3),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics netedev_metrics = {
	.count = NET_EPERINTF_METRIC_COUNT,
	.names = netedev_metric_names,
	.descs = netedev_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for serial lines statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_serial_metrics(struct activity *a)
{
	int i;
	char buf[64];

	/* Create serial metrics */
	act_add_metric(a, SERIAL_PERTTY_RX);
	act_add_metric(a, SERIAL_PERTTY_TX);
	act_add_metric(a, SERIAL_PERTTY_FRAME);
	act_add_metric(a, SERIAL_PERTTY_PARITY);
	act_add_metric(a, SERIAL_PERTTY_BRK);
	act_add_metric(a, SERIAL_PERTTY_OVERRUN);

	for (i = 0; i < a->nr_ini; i++) {
		/* Create instances */
		pmsprintf(buf, sizeof(buf), "serial%d", i);
		pmiAddInstance(PMI_INDOM(60, 35), buf, i);
	}
}

char *serial_metric_names[] = {
	[SERIAL_PERTTY_RX] = "tty.serial.rx",
	[SERIAL_PERTTY_TX] = "tty.serial.tx",
	[SERIAL_PERTTY_FRAME] = "tty.serial.frame",
	[SERIAL_PERTTY_PARITY] = "tty.serial.parity",
	[SERIAL_PERTTY_BRK] = "tty.serial.brk",
	[SERIAL_PERTTY_OVERRUN] = "tty.serial.overrun",
};

pmDesc serial_metric_descs[] = {
	[SERIAL_PERTTY_RX] = {
		.pmid = SERIAL_PERTTY_RX,
		.indom = PMI_INDOM(60, 35),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[SERIAL_PERTTY_TX] = {
		.pmid = SERIAL_PERTTY_TX,
		.indom = PMI_INDOM(60, 35),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[SERIAL_PERTTY_FRAME] = {
		.pmid = SERIAL_PERTTY_FRAME,
		.indom = PMI_INDOM(60, 35),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[SERIAL_PERTTY_PARITY] = {
		.pmid = SERIAL_PERTTY_PARITY,
		.indom = PMI_INDOM(60, 35),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[SERIAL_PERTTY_BRK] = {
		.pmid = SERIAL_PERTTY_BRK,
		.indom = PMI_INDOM(60, 35),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[SERIAL_PERTTY_OVERRUN] = {
		.pmid = SERIAL_PERTTY_OVERRUN,
		.indom = PMI_INDOM(60, 35),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics serial_metrics = {
	.count = SERIAL_METRIC_COUNT,
	.names = serial_metric_names,
	.descs = serial_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for NFS client statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_nfs_metrics(struct activity *a)
{
	pmInDom indom = PMI_INDOM(60, 7);

	pmiAddInstance(indom, "getattr", NFS_REQUEST_GETATTR);
	pmiAddInstance(indom, "read", NFS_REQUEST_READ);
	pmiAddInstance(indom, "write", NFS_REQUEST_WRITE);
	pmiAddInstance(indom, "access", NFS_REQUEST_ACCESS);

	act_add_metric(a, NFSCLIENT_RPCCCNT);
	act_add_metric(a, NFSCLIENT_RPCRETRANS);
	act_add_metric(a, NFSCLIENT_REQUESTS);
}

char *nfsclient_metric_names[] = {
	[NFSCLIENT_RPCCCNT] = "rpc.client.rpccnt",
	[NFSCLIENT_RPCRETRANS] = "rpc.client.rpcretrans",
	[NFSCLIENT_REQUESTS] = "nfs.client.reqs",
};

pmDesc nfsclient_metric_descs[] = {
	[NFSCLIENT_RPCCCNT] = {
		.pmid = PMID_NFSCLIENT_RPCCCNT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[NFSCLIENT_RPCRETRANS] = {
		.pmid = PMID_NFSCLIENT_RPCRETRANS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
	[NFSCLIENT_REQUESTS] = {
		.pmid = PMID_NFSCLIENT_REQUESTS,
		.indom = PMI_INDOM(60, 7),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics nfsclient_metrics = {
	.count = NFSCLIENT_METRIC_COUNT,
	.names = nfsclient_metric_names,
	.descs = nfsclient_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for NFS server statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_nfsd_metrics(struct activity *a)
{
	pmInDom indom = PMI_INDOM(60, 7);

	pmiAddInstance(indom, "getattr", NFS_REQUEST_GETATTR);
	pmiAddInstance(indom, "read", NFS_REQUEST_READ);
	pmiAddInstance(indom, "write", NFS_REQUEST_WRITE);
	pmiAddInstance(indom, "access", NFS_REQUEST_ACCESS);

	act_add_metric(a, NFSSERVER_RPCCNT);
	act_add_metric(a, NFSSERVER_RPCBADCLNT);
	act_add_metric(a, NFSSERVER_NETCNT);
	act_add_metric(a, NFSSERVER_NETUDPCNT);
	act_add_metric(a, NFSSERVER_NETTCPCNT);
	act_add_metric(a, NFSSERVER_RCHITS);
	act_add_metric(a, NFSSERVER_RCMISSES);
	act_add_metric(a, NFSSERVER_REQUESTS);
}

char *nfsserver_metric_names[] = {
	[NFSSERVER_RPCCNT] = "rpc.server.rpccnt",
	[NFSSERVER_RPCBADCLNT] = "rpc.server.rpcbadclnt",
	[NFSSERVER_NETCNT] = "rpc.server.netcnt",
	[NFSSERVER_NETUDPCNT] = "rpc.server.netudpcnt",
	[NFSSERVER_NETTCPCNT] = "rpc.server.nettcpcnt",
	[NFSSERVER_RCHITS] = "rpc.server.rchits",
	[NFSSERVER_RCMISSES] = "rpc.server.rcmisses",
	[NFSSERVER_REQUESTS] = "nfs.server.reqs",
};

pmDesc nfsserver_metric_descs[] = {
	[NFSSERVER_RPCCNT] = {
		.pmid = PMID_NFSSERVER_RPCCNT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NFSSERVER_RPCBADCLNT] = {
		.pmid = PMID_NFSSERVER_RPCBADCLNT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NFSSERVER_NETCNT] = {
		.pmid = PMID_NFSSERVER_NETCNT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NFSSERVER_NETUDPCNT] = {
		.pmid = PMID_NFSSERVER_NETUDPCNT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NFSSERVER_NETTCPCNT] = {
		.pmid = PMID_NFSSERVER_NETTCPCNT,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NFSSERVER_RCHITS] = {
		.pmid = PMID_NFSSERVER_RCHITS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NFSSERVER_RCMISSES] = {
		.pmid = PMID_NFSSERVER_RCMISSES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NFSSERVER_REQUESTS] = {
		.pmid = PMID_NFSSERVER_REQUESTS,
		.indom = PMI_INDOM(60, 7),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics nfsserver_metrics = {
	.count = NFSSERVER_METRIC_COUNT,
	.names = nfsserver_metric_names,
	.descs = nfsserver_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for network sockets statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_sock_metrics(struct activity *a)
{
	act_add_metric(a, SOCKET_TOTAL);
	act_add_metric(a, SOCKET_TCPINUSE);
	act_add_metric(a, SOCKET_UDPINUSE);
	act_add_metric(a, SOCKET_RAWINUSE);
	act_add_metric(a, SOCKET_FRAGINUSE);
	act_add_metric(a, SOCKET_TCPTW);
}

char *socket_metric_names[] = {
	[SOCKET_TOTAL] = "network.sockstat.total",
	[SOCKET_TCPINUSE] = "network.sockstat.tcp.inuse",
	[SOCKET_UDPINUSE] = "network.sockstat.udp.inuse",
	[SOCKET_RAWINUSE] = "network.sockstat.raw.inuse",
	[SOCKET_FRAGINUSE] = "network.sockstat.frag.inuse",
	[SOCKET_TCPTW] = "network.sockstat.tcp.tw",
};

pmDesc socket_metric_descs[] = {
	[SOCKET_TOTAL] = {
		.pmid = PMID_SOCKET_TOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[SOCKET_TCPINUSE] = {
		.pmid = PMID_SOCKET_TCPINUSE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[SOCKET_UDPINUSE] = {
		.pmid = PMID_SOCKET_UDPINUSE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[SOCKET_RAWINUSE] = {
		.pmid = PMID_SOCKET_RAWINUSE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[SOCKET_FRAGINUSE] = {
		.pmid = PMID_SOCKET_FRAGINUSE,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[SOCKET_TCPTW] = {
		.pmid = PMID_SOCKET_TCPTW,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
};

struct act_metrics socket_metrics = {
	.count = SOCKET_METRIC_COUNT,
	.names = socket_metric_names,
	.descs = socket_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for IP network statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_ip_metrics(struct activity *a)
{
	act_add_metric(a, NET_IP_INRECEIVES);
	act_add_metric(a, NET_IP_FORWDATAGRAMS);
	act_add_metric(a, NET_IP_INDELIVERS);
	act_add_metric(a, NET_IP_OUTREQUESTS);
	act_add_metric(a, NET_IP_REASMREQDS);
	act_add_metric(a, NET_IP_REASMOKS);
	act_add_metric(a, NET_IP_FRAGOKS);
	act_add_metric(a, NET_IP_FRAGCREATES);
}

char *net_ip_metric_names[] = {
	[NET_IP_INRECEIVES] = "network.ip.inreceives",
	[NET_IP_FORWDATAGRAMS] = "network.ip.forwdatagrams",
	[NET_IP_INDELIVERS] = "network.ip.indelivers",
	[NET_IP_OUTREQUESTS] = "network.ip.outrequests",
	[NET_IP_REASMREQDS] = "network.ip.reasmreqds",
	[NET_IP_REASMOKS] = "network.ip.reasmoks",
	[NET_IP_FRAGOKS] = "network.ip.fragoks",
	[NET_IP_FRAGCREATES] = "network.ip.fragcreates",
};

pmDesc net_ip_metric_descs[] = {
	[NET_IP_INRECEIVES] = {
		.pmid = PMID_NET_IP_INRECEIVES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP_FORWDATAGRAMS] = {
		.pmid = PMID_NET_IP_FORWDATAGRAMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP_INDELIVERS] = {
		.pmid = PMID_NET_IP_INDELIVERS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP_OUTREQUESTS] = {
		.pmid = PMID_NET_IP_OUTREQUESTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP_REASMREQDS] = {
		.pmid = PMID_NET_IP_REASMREQDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP_REASMOKS] = {
		.pmid = PMID_NET_IP_REASMOKS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP_FRAGOKS] = {
		.pmid = PMID_NET_IP_FRAGOKS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP_FRAGCREATES] = {
		.pmid = PMID_NET_IP_FRAGCREATES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics net_ip_metrics = {
	.count = NET_IP_METRIC_COUNT,
	.names = net_ip_metric_names,
	.descs = net_ip_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for IP network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_eip_metrics(struct activity *a)
{
	act_add_metric(a, NET_EIP_INHDRERRORS);
	act_add_metric(a, NET_EIP_INADDRERRORS);
	act_add_metric(a, NET_EIP_INUNKNOWNPROTOS);
	act_add_metric(a, NET_EIP_INDISCARDS);
	act_add_metric(a, NET_EIP_OUTDISCARDS);
	act_add_metric(a, NET_EIP_OUTNOROUTES);
	act_add_metric(a, NET_EIP_REASMFAILS);
	act_add_metric(a, NET_EIP_FRAGFAILS);
}

char *net_eip_metric_names[] = {
	[NET_EIP_INHDRERRORS] = "network.ip.inhdrerrors",
	[NET_EIP_INADDRERRORS] = "network.ip.inaddrerrors",
	[NET_EIP_INUNKNOWNPROTOS] = "network.ip.inunknownprotos",
	[NET_EIP_INDISCARDS] = "network.ip.indiscards",
	[NET_EIP_OUTDISCARDS] = "network.ip.outdiscards",
	[NET_EIP_OUTNOROUTES] = "network.ip.outnoroutes",
	[NET_EIP_REASMFAILS] = "network.ip.reasmfails",
	[NET_EIP_FRAGFAILS] = "network.ip.fragfails",
};

pmDesc net_eip_metric_descs[] = {
	[NET_EIP_INHDRERRORS] = {
		.pmid = PMID_NET_EIP_INHDRERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP_INADDRERRORS] = {
		.pmid = PMID_NET_EIP_INADDRERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP_INUNKNOWNPROTOS] = {
		.pmid = PMID_NET_EIP_INUNKNOWNPROTOS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP_INDISCARDS] = {
		.pmid = PMID_NET_EIP_INDISCARDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP_OUTDISCARDS] = {
		.pmid = PMID_NET_EIP_OUTDISCARDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP_OUTNOROUTES] = {
		.pmid = PMID_NET_EIP_OUTNOROUTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP_REASMFAILS] = {
		.pmid = PMID_NET_EIP_REASMFAILS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP_FRAGFAILS] = {
		.pmid = PMID_NET_EIP_FRAGFAILS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics net_eip_metrics = {
	.count = NET_EIP_METRIC_COUNT,
	.names = net_eip_metric_names,
	.descs = net_eip_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for ICMP network statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_icmp_metrics(struct activity *a)
{
	act_add_metric(a, NET_ICMP_INMSGS);
	act_add_metric(a, NET_ICMP_OUTMSGS);
	act_add_metric(a, NET_ICMP_INECHOS);
	act_add_metric(a, NET_ICMP_INECHOREPS);
	act_add_metric(a, NET_ICMP_OUTECHOS);
	act_add_metric(a, NET_ICMP_OUTECHOREPS);
	act_add_metric(a, NET_ICMP_INTIMESTAMPS);
	act_add_metric(a, NET_ICMP_INTIMESTAMPREPS);
	act_add_metric(a, NET_ICMP_OUTTIMESTAMPS);
	act_add_metric(a, NET_ICMP_OUTTIMESTAMPREPS);
	act_add_metric(a, NET_ICMP_INADDRMASKS);
	act_add_metric(a, NET_ICMP_INADDRMASKREPS);
	act_add_metric(a, NET_ICMP_OUTADDRMASKS);
	act_add_metric(a, NET_ICMP_OUTADDRMASKREPS);
}

char *net_icmp_metric_names[] = {
	[NET_ICMP_INMSGS] = "network.icmp.inmsgs",
	[NET_ICMP_OUTMSGS] = "network.icmp.outmsgs",
	[NET_ICMP_INECHOS] = "network.icmp.inechos",
	[NET_ICMP_INECHOREPS] = "network.icmp.inechoreps",
	[NET_ICMP_OUTECHOS] = "network.icmp.outechos",
	[NET_ICMP_OUTECHOREPS] = "network.icmp.outechoreps",
	[NET_ICMP_INTIMESTAMPS] = "network.icmp.intimestamps",
	[NET_ICMP_INTIMESTAMPREPS] = "network.icmp.intimestampreps",
	[NET_ICMP_OUTTIMESTAMPS] = "network.icmp.outtimestamps",
	[NET_ICMP_OUTTIMESTAMPREPS] = "network.icmp.outtimestampreps",
	[NET_ICMP_INADDRMASKS] = "network.icmp.inaddrmasks",
	[NET_ICMP_INADDRMASKREPS] = "network.icmp.inaddrmaskreps",
	[NET_ICMP_OUTADDRMASKS] = "network.icmp.outaddrmasks",
	[NET_ICMP_OUTADDRMASKREPS] = "network.icmp.outaddrmaskreps",
};

pmDesc net_icmp_metric_descs[] = {
	[NET_ICMP_INMSGS] = {
		.pmid = PMID_NET_ICMP_INMSGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_OUTMSGS] = {
		.pmid = PMID_NET_ICMP_OUTMSGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_INECHOS] = {
		.pmid = PMID_NET_ICMP_INECHOS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_INECHOREPS] = {
		.pmid = PMID_NET_ICMP_INECHOREPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_OUTECHOS] = {
		.pmid = PMID_NET_ICMP_OUTECHOS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_OUTECHOREPS] = {
		.pmid = PMID_NET_ICMP_OUTECHOREPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_INTIMESTAMPS] = {
		.pmid = PMID_NET_ICMP_INTIMESTAMPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_INTIMESTAMPREPS] = {
		.pmid = PMID_NET_ICMP_INTIMESTAMPREPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_OUTTIMESTAMPS] = {
		.pmid = PMID_NET_ICMP_OUTTIMESTAMPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_OUTTIMESTAMPREPS] = {
		.pmid = PMID_NET_ICMP_OUTTIMESTAMPREPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_INADDRMASKS] = {
		.pmid = PMID_NET_ICMP_INADDRMASKS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_INADDRMASKREPS] = {
		.pmid = PMID_NET_ICMP_INADDRMASKREPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_OUTADDRMASKS] = {
		.pmid = PMID_NET_ICMP_OUTADDRMASKS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP_OUTADDRMASKREPS] = {
		.pmid = PMID_NET_ICMP_OUTADDRMASKREPS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics net_icmp_metrics = {
	.count = NET_ICMP_METRIC_COUNT,
	.names = net_icmp_metric_names,
	.descs = net_icmp_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for ICMP network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_eicmp_metrics(struct activity *a)
{
	act_add_metric(a, NET_EICMP_INERRORS);
	act_add_metric(a, NET_EICMP_OUTERRORS);
	act_add_metric(a, NET_EICMP_INDESTUNREACHS);
	act_add_metric(a, NET_EICMP_OUTDESTUNREACHS);
	act_add_metric(a, NET_EICMP_INTIMEEXCDS);
	act_add_metric(a, NET_EICMP_OUTTIMEEXCDS);
	act_add_metric(a, NET_EICMP_INPARMPROBS);
	act_add_metric(a, NET_EICMP_OUTPARMPROBS);
	act_add_metric(a, NET_EICMP_INSRCQUENCHS);
	act_add_metric(a, NET_EICMP_OUTSRCQUENCHS);
	act_add_metric(a, NET_EICMP_INREDIRECTS);
	act_add_metric(a, NET_EICMP_OUTREDIRECTS);
}

char *net_eicmp_metric_names[] = {
	[NET_EICMP_INERRORS] = "network.icmp.inerrors",
	[NET_EICMP_OUTERRORS] = "network.icmp.outerrors",
	[NET_EICMP_INDESTUNREACHS] = "network.icmp.indestunreachs",
	[NET_EICMP_OUTDESTUNREACHS] = "network.icmp.outdestunreachs",
	[NET_EICMP_INTIMEEXCDS] = "network.icmp.intimeexcds",
	[NET_EICMP_OUTTIMEEXCDS] = "network.icmp.outtimeexcds",
	[NET_EICMP_INPARMPROBS] = "network.icmp.inparmprobs",
	[NET_EICMP_OUTPARMPROBS] = "network.icmp.outparmprobs",
	[NET_EICMP_INSRCQUENCHS] = "network.icmp.insrcquenchs",
	[NET_EICMP_OUTSRCQUENCHS] = "network.icmp.outsrcquenchs",
	[NET_EICMP_INREDIRECTS] = "network.icmp.inredirects",
	[NET_EICMP_OUTREDIRECTS] = "network.icmp.outredirects",
};

pmDesc net_eicmp_metric_descs[] = {
	[NET_EICMP_INERRORS] = {
		.pmid = PMID_NET_EICMP_INERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_OUTERRORS] = {
		.pmid = PMID_NET_EICMP_OUTERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_INDESTUNREACHS] = {
		.pmid = PMID_NET_EICMP_INDESTUNREACHS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_OUTDESTUNREACHS] = {
		.pmid = PMID_NET_EICMP_OUTDESTUNREACHS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_INTIMEEXCDS] = {
		.pmid = PMID_NET_EICMP_INTIMEEXCDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_OUTTIMEEXCDS] = {
		.pmid = PMID_NET_EICMP_OUTTIMEEXCDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_INPARMPROBS] = {
		.pmid = PMID_NET_EICMP_INPARMPROBS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_OUTPARMPROBS] = {
		.pmid = PMID_NET_EICMP_OUTPARMPROBS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_INSRCQUENCHS] = {
		.pmid = PMID_NET_EICMP_INSRCQUENCHS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_OUTSRCQUENCHS] = {
		.pmid = PMID_NET_EICMP_OUTSRCQUENCHS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_INREDIRECTS] = {
		.pmid = PMID_NET_EICMP_INREDIRECTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP_OUTREDIRECTS] = {
		.pmid = PMID_NET_EICMP_OUTREDIRECTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics net_eicmp_metrics = {
	.count = NET_EICMP_METRIC_COUNT,
	.names = net_eicmp_metric_names,
	.descs = net_eicmp_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for TCP network statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_tcp_metrics(struct activity *a)
{
	act_add_metric(a, NET_TCP_ACTIVEOPENS);
	act_add_metric(a, NET_TCP_PASSIVEOPENS);
	act_add_metric(a, NET_TCP_INSEGS);
	act_add_metric(a, NET_TCP_OUTSEGS);
}

char *net_tcp_metric_names[] = {
	[NET_TCP_ACTIVEOPENS] = "network.tcp.activeopens",
	[NET_TCP_PASSIVEOPENS] = "network.tcp.passiveopens",
	[NET_TCP_INSEGS] = "network.tcp.insegs",
	[NET_TCP_OUTSEGS] = "network.tcp.outsegs",
};

pmDesc net_tcp_metric_descs[] = {
	[NET_TCP_ACTIVEOPENS] = {
		.pmid = PMID_NET_TCP_ACTIVEOPENS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_TCP_PASSIVEOPENS] = {
		.pmid = PMID_NET_TCP_PASSIVEOPENS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_TCP_INSEGS] = {
		.pmid = PMID_NET_TCP_INSEGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_TCP_OUTSEGS] = {
		.pmid = PMID_NET_TCP_OUTSEGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics net_tcp_metrics = {
	.count = NET_TCP_METRIC_COUNT,
	.names = net_tcp_metric_names,
	.descs = net_tcp_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for TCP network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_etcp_metrics(struct activity *a)
{
	act_add_metric(a, NET_ETCP_ATTEMPTFAILS);
	act_add_metric(a, NET_ETCP_ESTABRESETS);
	act_add_metric(a, NET_ETCP_RETRANSSEGS);
	act_add_metric(a, NET_ETCP_INERRS);
	act_add_metric(a, NET_ETCP_OUTRSTS);
}

char *net_etcp_metric_names[] = {
	[NET_ETCP_ATTEMPTFAILS] = "network.tcp.attemptfails",
	[NET_ETCP_ESTABRESETS] = "network.tcp.estabresets",
	[NET_ETCP_RETRANSSEGS] = "network.tcp.retranssegs",
	[NET_ETCP_INERRS] = "network.tcp.inerrs",
	[NET_ETCP_OUTRSTS] = "network.tcp.outrsts",
};

pmDesc net_etcp_metric_descs[] = {
	[NET_ETCP_ATTEMPTFAILS] = {
		.pmid = PMID_NET_ETCP_ATTEMPTFAILS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ETCP_ESTABRESETS] = {
		.pmid = PMID_NET_ETCP_ESTABRESETS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ETCP_RETRANSSEGS] = {
		.pmid = PMID_NET_ETCP_RETRANSSEGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ETCP_INERRS] = {
		.pmid = PMID_NET_ETCP_INERRS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ETCP_OUTRSTS] = {
		.pmid = PMID_NET_ETCP_OUTRSTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics net_etcp_metrics = {
	.count = NET_ETCP_METRIC_COUNT,
	.names = net_etcp_metric_names,
	.descs = net_etcp_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for UDP network statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_udp_metrics(struct activity *a)
{
	act_add_metric(a, NET_UDP_INDATAGRAMS);
	act_add_metric(a, NET_UDP_OUTDATAGRAMS);
	act_add_metric(a, NET_UDP_NOPORTS);
	act_add_metric(a, NET_UDP_INERRORS);
}

char *net_udp_metric_names[] = {
	[NET_UDP_INDATAGRAMS] = "network.udp.indatagrams",
	[NET_UDP_OUTDATAGRAMS] = "network.udp.outdatagrams",
	[NET_UDP_NOPORTS] = "network.udp.noports",
	[NET_UDP_INERRORS] = "network.udp.inerrors",
};

pmDesc net_udp_metric_descs[] = {
	[NET_UDP_INDATAGRAMS] = {
		.pmid = PMID_NET_UDP_INDATAGRAMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_UDP_OUTDATAGRAMS] = {
		.pmid = PMID_NET_UDP_OUTDATAGRAMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_UDP_NOPORTS] = {
		.pmid = PMID_NET_UDP_NOPORTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_UDP_INERRORS] = {
		.pmid = PMID_NET_UDP_INERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics net_udp_metrics = {
	.count = NET_UDP_METRIC_COUNT,
	.names = net_udp_metric_names,
	.descs = net_udp_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for IPv6 network sockets statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_sock6_metrics(struct activity *a)
{
	act_add_metric(a, NET_SOCK6_TCPINUSE);
	act_add_metric(a, NET_SOCK6_UDPINUSE);
	act_add_metric(a, NET_SOCK6_RAWINUSE);
	act_add_metric(a, NET_SOCK6_FRAGINUSE);
}

char *net_sock6_metric_names[] = {
	[NET_SOCK6_TCPINUSE] = "network.sockstat.tcp6.inuse",
	[NET_SOCK6_UDPINUSE] = "network.sockstat.udp6.inuse",
	[NET_SOCK6_RAWINUSE] = "network.sockstat.raw6.inuse",
	[NET_SOCK6_FRAGINUSE] = "network.sockstat.frag6.inuse",
};

pmDesc net_sock6_metric_descs[] = {
	[NET_SOCK6_TCPINUSE] = {
		.pmid = PMI_ID(60, 73, 0),
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[NET_SOCK6_UDPINUSE] = {
		.pmid = PMI_ID(60, 73, 1),
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[NET_SOCK6_RAWINUSE] = {
		.pmid = PMI_ID(60, 73, 3),
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[NET_SOCK6_FRAGINUSE] = {
		.pmid = PMI_ID(60, 73, 4),
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
};

struct act_metrics net_sock6_metrics = {
	.count = NET_SOCK6_METRIC_COUNT,
	.names = net_sock6_metric_names,
	.descs = net_sock6_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for IPv6 network statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_ip6_metrics(struct activity *a)
{
	act_add_metric(a, NET_IP6_INRECEIVES);
	act_add_metric(a, NET_IP6_OUTFORWDATAGRAMS);
	act_add_metric(a, NET_IP6_INDELIVERS);
	act_add_metric(a, NET_IP6_OUTREQUESTS);
	act_add_metric(a, NET_IP6_REASMREQDS);
	act_add_metric(a, NET_IP6_REASMOKS);
	act_add_metric(a, NET_IP6_INMCASTPKTS);
	act_add_metric(a, NET_IP6_OUTMCASTPKTS);
	act_add_metric(a, NET_IP6_FRAGOKS);
	act_add_metric(a, NET_IP6_FRAGCREATES);
}

char *net_ip6_metric_names[] = {
	[NET_IP6_INRECEIVES] = "network.ip6.inreceives",
	[NET_IP6_OUTFORWDATAGRAMS] = "network.ip6.outforwdatagrams",
	[NET_IP6_INDELIVERS] = "network.ip6.indelivers",
	[NET_IP6_OUTREQUESTS] = "network.ip6.outrequests",
	[NET_IP6_REASMREQDS] = "network.ip6.reasmreqds",
	[NET_IP6_REASMOKS] = "network.ip6.reasmoks",
	[NET_IP6_INMCASTPKTS] = "network.ip6.inmcastpkts",
	[NET_IP6_OUTMCASTPKTS] = "network.ip6.outmcastpkts",
	[NET_IP6_FRAGOKS] = "network.ip6.fragoks",
	[NET_IP6_FRAGCREATES] = "network.ip6.fragcreates",
};

pmDesc net_ip6_metric_descs[] = {
	[NET_IP6_INRECEIVES] = {
		.pmid = PMID_NET_IP6_INRECEIVES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_OUTFORWDATAGRAMS] = {
		.pmid = PMID_NET_IP6_OUTFORWDATAGRAMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_INDELIVERS] = {
		.pmid = PMID_NET_IP6_INDELIVERS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_OUTREQUESTS] = {
		.pmid = PMID_NET_IP6_OUTREQUESTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_REASMREQDS] = {
		.pmid = PMID_NET_IP6_REASMREQDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_REASMOKS] = {
		.pmid = PMID_NET_IP6_REASMOKS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_INMCASTPKTS] = {
		.pmid = PMID_NET_IP6_INMCASTPKTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_OUTMCASTPKTS] = {
		.pmid = PMID_NET_IP6_OUTMCASTPKTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_FRAGOKS] = {
		.pmid = PMID_NET_IP6_FRAGOKS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_IP6_FRAGCREATES] = {
		.pmid = PMID_NET_IP6_FRAGCREATES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics net_ip6_metrics = {
	.count = NET_IP6_METRIC_COUNT,
	.names = net_ip6_metric_names,
	.descs = net_ip6_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for IPv6 network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_eip6_metrics(struct activity *a)
{
	act_add_metric(a, NET_EIP6_INHDRERRORS);
	act_add_metric(a, NET_EIP6_INADDRERRORS);
	act_add_metric(a, NET_EIP6_INUNKNOWNPROTOS);
	act_add_metric(a, NET_EIP6_INTOOBIGERRORS);
	act_add_metric(a, NET_EIP6_INDISCARDS);
	act_add_metric(a, NET_EIP6_OUTDISCARDS);
	act_add_metric(a, NET_EIP6_INNOROUTES);
	act_add_metric(a, NET_EIP6_OUTNOROUTES);
	act_add_metric(a, NET_EIP6_REASMFAILS);
	act_add_metric(a, NET_EIP6_FRAGFAILS);
	act_add_metric(a, NET_EIP6_INTRUNCATEDPKTS );
}

char *net_eip6_metric_names[] = {
	[NET_EIP6_INHDRERRORS] = "network.ip6.inhdrerrors",
	[NET_EIP6_INADDRERRORS] = "network.ip6.inaddrerrors",
	[NET_EIP6_INUNKNOWNPROTOS] = "network.ip6.inunknownprotos",
	[NET_EIP6_INTOOBIGERRORS] = "network.ip6.intoobigerrors",
	[NET_EIP6_INDISCARDS] = "network.ip6.indiscards",
	[NET_EIP6_OUTDISCARDS] = "network.ip6.outdiscards",
	[NET_EIP6_INNOROUTES] = "network.ip6.innoroutes",
	[NET_EIP6_OUTNOROUTES] = "network.ip6.outnoroutes",
	[NET_EIP6_REASMFAILS] = "network.ip6.reasmfails",
	[NET_EIP6_FRAGFAILS] = "network.ip6.fragfails",
	[NET_EIP6_INTRUNCATEDPKTS] = "network.ip6.intruncatedpkts",
};

pmDesc net_eip6_metric_descs[] = {
	[NET_EIP6_INHDRERRORS] = {
		.pmid = PMID_NET_EIP6_INHDRERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_INADDRERRORS] = {
		.pmid = PMID_NET_EIP6_INADDRERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_INUNKNOWNPROTOS] = {
		.pmid = PMID_NET_EIP6_INUNKNOWNPROTOS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_INTOOBIGERRORS] = {
		.pmid = PMID_NET_EIP6_INTOOBIGERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_INDISCARDS] = {
		.pmid = PMID_NET_EIP6_INDISCARDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_OUTDISCARDS] = {
		.pmid = PMID_NET_EIP6_OUTDISCARDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_INNOROUTES] = {
		.pmid = PMID_NET_EIP6_INNOROUTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_OUTNOROUTES] = {
		.pmid = PMID_NET_EIP6_OUTNOROUTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_REASMFAILS] = {
		.pmid = PMID_NET_EIP6_REASMFAILS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_FRAGFAILS] = {
		.pmid = PMID_NET_EIP6_FRAGFAILS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EIP6_INTRUNCATEDPKTS] = {
		.pmid = PMID_NET_EIP6_INTRUNCATEDPKTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics net_eip6_metrics = {
	.count = NET_EIP6_METRIC_COUNT,
	.names = net_eip6_metric_names,
	.descs = net_eip6_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for ICMPv6 network statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_icmp6_metrics(struct activity *a)
{
	act_add_metric(a, NET_ICMP6_INMSGS);
	act_add_metric(a, NET_ICMP6_OUTMSGS);
	act_add_metric(a, NET_ICMP6_INECHOS);
	act_add_metric(a, NET_ICMP6_INECHOREPLIES);
	act_add_metric(a, NET_ICMP6_OUTECHOREPLIES);
	act_add_metric(a, NET_ICMP6_INGROUPMEMBQUERIES);
	act_add_metric(a, NET_ICMP6_INGROUPMEMBRESPONSES);
	act_add_metric(a, NET_ICMP6_OUTGROUPMEMBRESPONSES);
	act_add_metric(a, NET_ICMP6_INGROUPMEMBREDUCTIONS);
	act_add_metric(a, NET_ICMP6_OUTGROUPMEMBREDUCTIONS);
	act_add_metric(a, NET_ICMP6_INROUTERSOLICITS);
	act_add_metric(a, NET_ICMP6_OUTROUTERSOLICITS);
	act_add_metric(a, NET_ICMP6_INROUTERADVERTISEMENTS);
	act_add_metric(a, NET_ICMP6_INNEIGHBORSOLICITS);
	act_add_metric(a, NET_ICMP6_OUTNEIGHBORSOLICITS);
	act_add_metric(a, NET_ICMP6_INNEIGHBORADVERTISEMENTS);
	act_add_metric(a, NET_ICMP6_OUTNEIGHBORADVERTISEMENTS);
}

char *net_icmp6_metric_names[] = {
	[NET_ICMP6_INMSGS] = "network.icmp6.inmsgs",
	[NET_ICMP6_OUTMSGS] = "network.icmp6.outmsgs",
	[NET_ICMP6_INECHOS] = "network.icmp6.inechos",
	[NET_ICMP6_INECHOREPLIES] = "network.icmp6.inechoreplies",
	[NET_ICMP6_OUTECHOREPLIES] = "network.icmp6.outechoreplies",
	[NET_ICMP6_INGROUPMEMBQUERIES] = "network.icmp6.ingroupmembqueries",
	[NET_ICMP6_INGROUPMEMBRESPONSES] = "network.icmp6.ingroupmembresponses",
	[NET_ICMP6_OUTGROUPMEMBRESPONSES] = "network.icmp6.outgroupmembresponses",
	[NET_ICMP6_INGROUPMEMBREDUCTIONS] = "network.icmp6.ingroupmembreductions",
	[NET_ICMP6_OUTGROUPMEMBREDUCTIONS] = "network.icmp6.outgroupmembreductions",
	[NET_ICMP6_INROUTERSOLICITS] = "network.icmp6.inroutersolicits",
	[NET_ICMP6_OUTROUTERSOLICITS] = "network.icmp6.outroutersolicits",
	[NET_ICMP6_INROUTERADVERTISEMENTS] = "network.icmp6.inrouteradvertisements",
	[NET_ICMP6_INNEIGHBORSOLICITS] = "network.icmp6.inneighborsolicits",
	[NET_ICMP6_OUTNEIGHBORSOLICITS] = "network.icmp6.outneighborsolicits",
	[NET_ICMP6_INNEIGHBORADVERTISEMENTS] = "network.icmp6.inneighboradvertisements",
	[NET_ICMP6_OUTNEIGHBORADVERTISEMENTS] = "network.icmp6.outneighboradvertisements",
};

pmDesc net_icmp6_metric_descs[] = {
	[NET_ICMP6_INMSGS] = {
		.pmid = PMID_NET_ICMP6_INMSGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_OUTMSGS] = {
		.pmid = PMID_NET_ICMP6_OUTMSGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INECHOS] = {
		.pmid = PMID_NET_ICMP6_INECHOS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INECHOREPLIES] = {
		.pmid = PMID_NET_ICMP6_INECHOREPLIES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_OUTECHOREPLIES] = {
		.pmid = PMID_NET_ICMP6_OUTECHOREPLIES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INGROUPMEMBQUERIES] = {
		.pmid = PMID_NET_ICMP6_INGROUPMEMBQUERIES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INGROUPMEMBRESPONSES] = {
		.pmid = PMID_NET_ICMP6_INGROUPMEMBRESPONSES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_OUTGROUPMEMBRESPONSES] = {
		.pmid = PMID_NET_ICMP6_OUTGROUPMEMBRESPONSES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INGROUPMEMBREDUCTIONS] = {
		.pmid = PMID_NET_ICMP6_INGROUPMEMBREDUCTIONS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_OUTGROUPMEMBREDUCTIONS] = {
		.pmid = PMID_NET_ICMP6_OUTGROUPMEMBREDUCTIONS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INROUTERSOLICITS] = {
		.pmid = PMID_NET_ICMP6_INROUTERSOLICITS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_OUTROUTERSOLICITS] = {
		.pmid = PMID_NET_ICMP6_OUTROUTERSOLICITS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INROUTERADVERTISEMENTS] = {
		.pmid = PMID_NET_ICMP6_INROUTERADVERTISEMENTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INNEIGHBORSOLICITS] = {
		.pmid = PMID_NET_ICMP6_INNEIGHBORSOLICITS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_OUTNEIGHBORSOLICITS] = {
		.pmid = PMID_NET_ICMP6_OUTNEIGHBORSOLICITS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_INNEIGHBORADVERTISEMENTS] = {
		.pmid = PMID_NET_ICMP6_INNEIGHBORADVERTISEMENTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_ICMP6_OUTNEIGHBORADVERTISEMENTS] = {
		.pmid = PMID_NET_ICMP6_OUTNEIGHBORADVERTISEMENTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics net_icmp6_metrics = {
	.count = NET_ICMP6_METRIC_COUNT,
	.names = net_icmp6_metric_names,
	.descs = net_icmp6_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for ICMPv6 network errors statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_eicmp6_metrics(struct activity *a)
{
	act_add_metric(a, NET_EICMP6_INERRORS);
	act_add_metric(a, NET_EICMP6_INDESTUNREACHS);
	act_add_metric(a, NET_EICMP6_OUTDESTUNREACHS);
	act_add_metric(a, NET_EICMP6_INTIMEEXCDS);
	act_add_metric(a, NET_EICMP6_OUTTIMEEXCDS);
	act_add_metric(a, NET_EICMP6_INPARMPROBLEMS);
	act_add_metric(a, NET_EICMP6_OUTPARMPROBLEMS);
	act_add_metric(a, NET_EICMP6_INREDIRECTS);
	act_add_metric(a, NET_EICMP6_OUTREDIRECTS);
	act_add_metric(a, NET_EICMP6_INPKTTOOBIGS);
	act_add_metric(a, NET_EICMP6_OUTPKTTOOBIGS);
}

char *net_eicmp6_metric_names[] = {
	[NET_EICMP6_INERRORS] = "network.icmp6.inerrors",
	[NET_EICMP6_INDESTUNREACHS] = "network.icmp6.indestunreachs",
	[NET_EICMP6_OUTDESTUNREACHS] = "network.icmp6.outdestunreachs",
	[NET_EICMP6_OUTTIMEEXCDS] = "network.icmp6.outtimeexcds",
	[NET_EICMP6_INPARMPROBLEMS] = "network.icmp6.inparmproblems",
	[NET_EICMP6_OUTPARMPROBLEMS] = "network.icmp6.outparmproblems",
	[NET_EICMP6_INTIMEEXCDS] = "network.icmp6.intimeexcds",
	[NET_EICMP6_INREDIRECTS] = "network.icmp6.inredirects",
	[NET_EICMP6_OUTREDIRECTS] = "network.icmp6.outredirects",
	[NET_EICMP6_INPKTTOOBIGS] = "network.icmp6.inpkttoobigs",
	[NET_EICMP6_OUTPKTTOOBIGS] = "network.icmp6.outpkttoobigs",
};

pmDesc net_eicmp6_metric_descs[] = {
	[NET_EICMP6_INERRORS] = {
		.pmid = PMID_NET_EICMP6_INERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_INDESTUNREACHS] = {
		.pmid = PMID_NET_EICMP6_INDESTUNREACHS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_OUTDESTUNREACHS] = {
		.pmid = PMID_NET_EICMP6_OUTDESTUNREACHS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_INTIMEEXCDS] = {
		.pmid = PMID_NET_EICMP6_INTIMEEXCDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_OUTTIMEEXCDS] = {
		.pmid = PMID_NET_EICMP6_OUTTIMEEXCDS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_INPARMPROBLEMS] = {
		.pmid = PMID_NET_EICMP6_INPARMPROBLEMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_OUTPARMPROBLEMS] = {
		.pmid = PMID_NET_EICMP6_OUTPARMPROBLEMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_INREDIRECTS] = {
		.pmid = PMID_NET_EICMP6_INREDIRECTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_OUTREDIRECTS] = {
		.pmid = PMID_NET_EICMP6_OUTREDIRECTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_INPKTTOOBIGS] = {
		.pmid = PMID_NET_EICMP6_INPKTTOOBIGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_EICMP6_OUTPKTTOOBIGS] = {
		.pmid = PMID_NET_EICMP6_OUTPKTTOOBIGS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics net_eicmp6_metrics = {
	.count = NET_EICMP6_METRIC_COUNT,
	.names = net_eicmp6_metric_names,
	.descs = net_eicmp6_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for UDPv6 network statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_net_udp6_metrics(struct activity *a)
{
	act_add_metric(a, NET_UDP6_INDATAGRAMS);
	act_add_metric(a, NET_UDP6_OUTDATAGRAMS);
	act_add_metric(a, NET_UDP6_NOPORTS);
	act_add_metric(a, NET_UDP6_INERRORS);
}

char *net_udp6_metric_names[] = {
	[NET_UDP6_INDATAGRAMS] = "network.udp6.indatagrams",
	[NET_UDP6_OUTDATAGRAMS] = "network.udp6.outdatagrams",
	[NET_UDP6_NOPORTS] = "network.udp6.noports",
	[NET_UDP6_INERRORS] = "network.udp6.inerrors",
};

pmDesc net_udp6_metric_descs[] = {
	[NET_UDP6_INDATAGRAMS] = {
		.pmid = PMID_NET_UDP6_INDATAGRAMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_UDP6_OUTDATAGRAMS] = {
		.pmid = PMID_NET_UDP6_OUTDATAGRAMS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_UDP6_NOPORTS] = {
		.pmid = PMID_NET_UDP6_NOPORTS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[NET_UDP6_INERRORS] = {
		.pmid = PMID_NET_UDP6_INERRORS,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics net_udp6_metrics = {
	.count = NET_UDP6_METRIC_COUNT,
	.names = net_udp6_metric_names,
	.descs = net_udp6_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for huge pages statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_huge_metrics(struct activity *a)
{
	act_add_metric(a, MEM_HUGE_TOTALBYTES);
	act_add_metric(a, MEM_HUGE_FREEBYTES);
	act_add_metric(a, MEM_HUGE_RSVDBYTES);
	act_add_metric(a, MEM_HUGE_SURPBYTES);
}

char *mem_huge_metric_names[] = {
	[MEM_HUGE_TOTALBYTES] = "mem.util.hugepagesTotalBytes",
	[MEM_HUGE_FREEBYTES] = "mem.util.hugepagesFreeBytes",
	[MEM_HUGE_RSVDBYTES] = "mem.util.hugepagesRsvdBytes",
	[MEM_HUGE_SURPBYTES] = "mem.util.hugepagesSurpBytes",
};

pmDesc mem_huge_metric_descs[] = {
	[MEM_HUGE_TOTALBYTES] = {
		.pmid = PMID_MEM_HUGE_TOTALBYTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_HUGE_FREEBYTES] = {
		.pmid = PMID_MEM_HUGE_FREEBYTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_HUGE_RSVDBYTES] = {
		.pmid = PMID_MEM_HUGE_RSVDBYTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[MEM_HUGE_SURPBYTES] = {
		.pmid = PMID_MEM_HUGE_SURPBYTES,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
};

struct act_metrics mem_huge_metrics = {
	.count = MEM_HUGE_METRIC_COUNT,
	.names = mem_huge_metric_names,
	.descs = mem_huge_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for fan statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_fan_metrics(struct activity *a)
{
	int inst = 0;
	static int setup;
	char buf[16];

	if (!setup) {
		setup = 1;

		/* Create domain */
		for (inst = 0; inst < a->item_list_sz; inst++) {
			pmsprintf(buf, sizeof(buf), "fan%d", inst + 1);
			pmiAddInstance(PMI_INDOM(34, 0), buf, inst);
		}
	}

	act_add_metric(a, POWER_FAN_RPM);
	act_add_metric(a, POWER_FAN_DRPM);
	act_add_metric(a, POWER_FAN_DEVICE);
}

char *power_fan_metric_names[] = {
	[POWER_FAN_RPM] = "power.fan.rpm",
	[POWER_FAN_DRPM] = "power.fan.drpm",
	[POWER_FAN_DEVICE] = "power.fan.device",
};

pmDesc power_fan_metric_descs[] = {
	[POWER_FAN_RPM] = {
		.pmid = PMID_POWER_FAN_RPM,
		.indom = PMI_INDOM(34, 0),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[POWER_FAN_DRPM] = {
		.pmid = PMID_POWER_FAN_DRPM,
		.indom = PMI_INDOM(34, 0),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[POWER_FAN_DEVICE] = {
		.pmid = PMID_POWER_FAN_DEVICE,
		.indom = PMI_INDOM(34, 0),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
};

struct act_metrics power_fan_metrics = {
	.count = POWER_FAN_METRIC_COUNT,
	.names = power_fan_metric_names,
	.descs = power_fan_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for temperature statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_temp_metrics(struct activity *a)
{
	int inst = 0;
	static int setup;
	char buf[16];

	if (!setup) {
		setup = 1;

		/* Create domain */
		for (inst = 0; inst < a->item_list_sz; inst++) {
			pmsprintf(buf, sizeof(buf), "temp%d", inst + 1);
			pmiAddInstance(PMI_INDOM(34, 1), buf, inst);
		}
	}
	act_add_metric(a, POWER_TEMP_CELSIUS);
	act_add_metric(a, POWER_TEMP_PERCENT);
	act_add_metric(a, POWER_TEMP_DEVICE);
}

char *power_temp_metric_names[] = {
	[POWER_TEMP_CELSIUS] = "power.temp.celsius",
	[POWER_TEMP_PERCENT] = "power.temp.percent",
	[POWER_TEMP_DEVICE] = "power.temp.device",
};

pmDesc power_temp_metric_descs[] = {
	[POWER_TEMP_CELSIUS] = {
		.pmid = PMID_POWER_TEMP_CELSIUS,
		.indom = PMI_INDOM(34, 1),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
	[POWER_TEMP_PERCENT] = {
		.pmid = PMID_POWER_TEMP_PERCENT,
		.indom = PMI_INDOM(34, 1),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
	[POWER_TEMP_DEVICE] = {
		.pmid = PMID_POWER_TEMP_DEVICE,
		.indom = PMI_INDOM(34, 1),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
};

struct act_metrics power_temp_metrics = {
	.count = POWER_TEMP_METRIC_COUNT,
	.names = power_temp_metric_names,
	.descs = power_temp_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for voltage inputs statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_in_metrics(struct activity *a)
{
	int inst = 0;
	static int setup;
	char buf[16];

	if (!setup) {
		setup = 1;

		/* Create domain */
		for (inst = 0; inst < a->item_list_sz; inst++) {
			pmsprintf(buf, sizeof(buf), "in%d", inst);
			pmiAddInstance(PMI_INDOM(34, 2), buf, inst);
		}
	}

	act_add_metric(a, POWER_IN_VOLTAGE);
	act_add_metric(a, POWER_IN_PERCENT);
	act_add_metric(a, POWER_IN_DEVICE);
}

char *power_in_metric_names[] = {
	[POWER_IN_VOLTAGE] = "power.in.voltage",
	[POWER_IN_PERCENT] = "power.in.percent",
	[POWER_IN_DEVICE] = "power.in.device",
};

pmDesc power_in_metric_descs[] = {
	[POWER_IN_VOLTAGE] = {
		.pmid = PMID_POWER_IN_VOLTAGE,
		.indom = PMI_INDOM(34, 2),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
	[POWER_IN_PERCENT] = {
		.pmid = PMID_POWER_IN_PERCENT,
		.indom = PMI_INDOM(34, 2),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
	[POWER_IN_DEVICE] = {
		.pmid = PMID_POWER_IN_DEVICE,
		.indom = PMI_INDOM(34, 2),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
};

struct act_metrics power_in_metrics = {
	.count = POWER_IN_METRIC_COUNT,
	.names = power_in_metric_names,
	.descs = power_in_metric_descs,
};

/*
 * **************************************************************************
 * Define PCP metrics for battery statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_bat_metrics(struct activity *a)
{
	int inst = 0;
	static int setup;
	struct sa_item *list;

	if (!setup) {
		setup = 1;

		/* Create instances */
		for (list = a->item_list; list != NULL; list = list->next) {
			pmiAddInstance(PMI_INDOM(34, 4), list->item_name, inst++);
		}
	}

	act_add_metric(a, POWER_BAT_CAPACITY);
	act_add_metric(a, POWER_BAT_STATUS);
}

char *power_bat_metric_names[] = {
	[POWER_BAT_CAPACITY] = "power.bat.capacity",
	[POWER_BAT_STATUS] = "power.bat.status",
};

pmDesc power_bat_metric_descs[] = {
	[POWER_BAT_CAPACITY] = {
		.pmid = PMID_POWER_BAT_CAPACITY,
		.indom = PMI_INDOM(34, 4),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_INSTANT,
	},
	[POWER_BAT_STATUS] = {
		.pmid = PMID_POWER_BAT_STATUS,
		.indom = PMI_INDOM(34, 4),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_INSTANT,
	},
};

struct act_metrics power_bat_metrics = {
	.count = POWER_BAT_METRIC_COUNT,
	.names = power_bat_metric_names,
	.descs = power_bat_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for USB devices statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_pwr_usb_metrics(struct activity *a)
{
	int inst = 0;
	static int setup;
	char buf[16];

	if (!setup) {
		setup = 1;

		/* Create domain */
		for (inst = 0; inst < a->item_list_sz; inst++) {
			pmsprintf(buf, sizeof(buf), "usb%d", inst);
			pmiAddInstance(PMI_INDOM(34, 3), buf, inst);
		}
	}

	act_add_metric(a, POWER_USB_BUS);
	act_add_metric(a, POWER_USB_VENDORID);
	act_add_metric(a, POWER_USB_PRODUCTID);
	act_add_metric(a, POWER_USB_MAXPOWER);
	act_add_metric(a, POWER_USB_MANUFACTURER);
	act_add_metric(a, POWER_USB_PRODUCTNAME);
}

char *power_usb_metric_names[] = {
	[POWER_USB_BUS] = "power.usb.bus",
	[POWER_USB_VENDORID] = "power.usb.vendorId",
	[POWER_USB_PRODUCTID] = "power.usb.productId",
	[POWER_USB_MAXPOWER] = "power.usb.maxpower",
	[POWER_USB_MANUFACTURER] = "power.usb.manufacturer",
	[POWER_USB_PRODUCTNAME] = "power.usb.productName",
};

pmDesc power_usb_metric_descs[] = {
	[POWER_USB_BUS] = {
		.pmid = PMID_POWER_USB_BUS,
		.indom = PMI_INDOM(34, 3),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_DISCRETE,
	},
	[POWER_USB_VENDORID] = {
		.pmid = PMID_POWER_USB_VENDORID,
		.indom = PMI_INDOM(34, 3),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
	[POWER_USB_PRODUCTID] = {
		.pmid = PMID_POWER_USB_PRODUCTID,
		.indom = PMI_INDOM(34, 3),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
	[POWER_USB_MAXPOWER] = {
		.pmid = PMID_POWER_USB_MAXPOWER,
		.indom = PMI_INDOM(34, 3),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_U32,
		.sem = PM_SEM_DISCRETE,
	},
	[POWER_USB_MANUFACTURER] = {
		.pmid = PMID_POWER_USB_MANUFACTURER,
		.indom = PMI_INDOM(34, 3),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
	[POWER_USB_PRODUCTNAME] = {
		.pmid = PMID_POWER_USB_PRODUCTNAME,
		.indom = PMI_INDOM(34, 3),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_STRING,
		.sem = PM_SEM_DISCRETE,
	},
};

struct act_metrics power_usb_metrics = {
	.count = POWER_USB_METRIC_COUNT,
	.names = power_usb_metric_names,
	.descs = power_usb_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for filesystem statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_filesystem_metrics(struct activity *a)
{
	int inst = 0;
	static int setup;
	struct sa_item *list;

	if (!setup) {
		setup = 1;

		/* Create instances */
		for (list = a->item_list; list != NULL; list = list->next) {
			pmiAddInstance(PMI_INDOM(60, 5), list->item_name, inst++);
		}
	}

	act_add_metric(a, FILESYS_CAPACITY);
	act_add_metric(a, FILESYS_FREE);
	act_add_metric(a, FILESYS_USED);
	act_add_metric(a, FILESYS_FULL);
	act_add_metric(a, FILESYS_MAXFILES);
	act_add_metric(a, FILESYS_FREEFILES);
	act_add_metric(a, FILESYS_USEDFILES);
	act_add_metric(a, FILESYS_AVAIL);
}

char *filesys_metric_names[] = {
	[FILESYS_CAPACITY] = "filesys.capacity",
	[FILESYS_FREE] = "filesys.free",
	[FILESYS_USED] = "filesys.used",
	[FILESYS_FULL] = "filesys.full",
	[FILESYS_MAXFILES] = "filesys.maxfiles",
	[FILESYS_FREEFILES] = "filesys.freefiles",
	[FILESYS_USEDFILES] = "filesys.usedfiles",
	[FILESYS_AVAIL] = "filesys.avail",
};

pmDesc filesys_metric_descs[] = {
	[FILESYS_CAPACITY] = {
		.pmid = PMID_FILESYS_CAPACITY,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[FILESYS_FREE] = {
		.pmid = PMID_FILESYS_FREE,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[FILESYS_USED] = {
		.pmid = PMID_FILESYS_USED,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[FILESYS_FULL] = {
		.pmid = PMID_FILESYS_FULL,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_DOUBLE,
		.sem = PM_SEM_INSTANT,
	},
	[FILESYS_MAXFILES] = {
		.pmid = PMID_FILESYS_MAXFILES,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[FILESYS_FREEFILES] = {
		.pmid = PMID_FILESYS_FREEFILES,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[FILESYS_USEDFILES] = {
		.pmid = PMID_FILESYS_USEDFILES,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
	[FILESYS_AVAIL] = {
		.pmid = PMID_FILESYS_AVAIL,
		.indom = PMI_INDOM(60, 5),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_INSTANT,
	},
};

struct act_metrics filesys_metrics = {
	.count = FILESYS_METRIC_COUNT,
	.names = filesys_metric_names,
	.descs = filesys_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for Fibre Channel HBA statistics.
 *
 * IN:
 * @a		Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_fchost_metrics(struct activity *a)
{
	int inst = 0;
	static int setup;
	struct sa_item *list;

	if (!setup) {
		setup = 1;

		/* Create instances */
		for (list = a->item_list; list != NULL; list = list->next) {
			pmiAddInstance(PMI_INDOM(60, 39), list->item_name, inst++);
		}
	}

	act_add_metric(a, FCHOST_INFRAMES);
	act_add_metric(a, FCHOST_OUTFRAMES);
	act_add_metric(a, FCHOST_INBYTES);
	act_add_metric(a, FCHOST_OUTBYTES);
}

char *fchost_metric_names[] = {
	[FCHOST_INFRAMES] = "fchost.in.frames",
	[FCHOST_OUTFRAMES] = "fchost.out.frames",
	[FCHOST_INBYTES] = "fchost.in.bytes",
	[FCHOST_OUTBYTES] = "fchost.out.bytes",
};

pmDesc fchost_metric_descs[] = {
	[FCHOST_INFRAMES] = {
		.pmid = PMID_FCHOST_INFRAMES,
		.indom = PMI_INDOM(60, 39),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[FCHOST_OUTFRAMES] = {
		.pmid = PMID_FCHOST_OUTFRAMES,
		.indom = PMI_INDOM(60, 39),
		.units = PMI_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[FCHOST_INBYTES] = {
		.pmid = PMID_FCHOST_INBYTES,
		.indom = PMI_INDOM(60, 39),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[FCHOST_OUTBYTES] = {
		.pmid = PMID_FCHOST_OUTBYTES,
		.indom = PMI_INDOM(60, 39),
		.units = PMI_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
};

struct act_metrics fchost_metrics = {
	.count = FCHOST_METRIC_COUNT,
	.names = fchost_metric_names,
	.descs = fchost_metric_descs,
};

/*
 ***************************************************************************
 * Define PCP metrics for pressure-stall statistics.
 *
 * IN:
 * @a	Activity structure with statistics.
 ***************************************************************************
 */
void pcp_def_psi_metrics(struct activity *a)
{
	static int setup;

	if (!setup) {
		setup = 1;
		pmiAddInstance(PMI_INDOM(60, 37), "10 second", 10);
		pmiAddInstance(PMI_INDOM(60, 37), "1 minute", 60);
		pmiAddInstance(PMI_INDOM(60, 37), "5 minute", 300);
	}

	if (a->id == A_PSI_CPU) {
		/* Create metrics for A_PSI_CPU */
		act_add_metric(a, PSI_CPU_SOMETOTAL);
		act_add_metric(a, PSI_CPU_SOMEAVG);
	}
	else if (a->id == A_PSI_IO) {
		/* Create metrics for A_PSI_IO */
		act_add_metric(a, PSI_IO_SOMETOTAL);
		act_add_metric(a, PSI_IO_SOMEAVG);
		act_add_metric(a, PSI_IO_FULLTOTAL);
		act_add_metric(a, PSI_IO_FULLAVG);
	}
	else if (a->id == A_PSI_MEM) {
		/* Create metrics for A_PSI_MEM */
		act_add_metric(a, PSI_MEM_SOMETOTAL);
		act_add_metric(a, PSI_MEM_SOMEAVG);
		act_add_metric(a, PSI_MEM_FULLTOTAL);
		act_add_metric(a, PSI_MEM_FULLAVG);
	}
}

char *psi_cpu_metric_names[] = {
	[PSI_CPU_SOMETOTAL] = "kernel.all.pressure.cpu.some.total",
	[PSI_CPU_SOMEAVG] = "kernel.all.pressure.cpu.some.avg",
};

pmDesc psi_cpu_metric_descs[] = {
	[PSI_CPU_SOMETOTAL] = {
		.pmid = PMID_PSI_CPU_SOMETOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_USEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PSI_CPU_SOMEAVG] = {
		.pmid = PMID_PSI_CPU_SOMEAVG,
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
};

struct act_metrics psi_cpu_metrics = {
	.count = PSI_CPU_METRIC_COUNT,
	.names = psi_cpu_metric_names,
	.descs = psi_cpu_metric_descs,
};

char *psi_io_metric_names[] = {
	[PSI_IO_SOMETOTAL] = "kernel.all.pressure.io.some.total",
	[PSI_IO_SOMEAVG] = "kernel.all.pressure.io.some.avg",
	[PSI_IO_FULLTOTAL] = "kernel.all.pressure.io.full.total",
	[PSI_IO_FULLAVG] = "kernel.all.pressure.io.full.avg",
};

pmDesc psi_io_metric_descs[] = {
	[PSI_IO_SOMETOTAL] = {
		.pmid = PMID_PSI_IO_SOMETOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_USEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PSI_IO_SOMEAVG] = {
		.pmid = PMID_PSI_IO_SOMEAVG,
		.indom = PMI_INDOM(60, 37),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
	[PSI_IO_FULLTOTAL] = {
		.pmid = PMID_PSI_IO_FULLTOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_USEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PSI_IO_FULLAVG] = {
		.pmid = PMID_PSI_IO_FULLAVG,
		.indom = PMI_INDOM(60, 37),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
};

struct act_metrics psi_io_metrics = {
	.count = PSI_IO_METRIC_COUNT,
	.names = psi_io_metric_names,
	.descs = psi_io_metric_descs,
};

char *psi_mem_metric_names[] = {
	[PSI_MEM_SOMETOTAL] = "kernel.all.pressure.mem.some.total",
	[PSI_MEM_SOMEAVG] = "kernel.all.pressure.mem.some.avg",
	[PSI_MEM_FULLTOTAL] = "kernel.all.pressure.mem.full.total",
	[PSI_MEM_FULLAVG] = "kernel.all.pressure.mem.full.avg",
};

pmDesc psi_mem_metric_descs[] = {
	[PSI_MEM_SOMETOTAL] = {
		.pmid = PMID_PSI_MEM_SOMETOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_USEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PSI_MEM_SOMEAVG] = {
		.pmid = PMID_PSI_MEM_SOMEAVG,
		.indom = PMI_INDOM(60, 37),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
	[PSI_MEM_FULLTOTAL] = {
		.pmid = PMID_PSI_MEM_FULLTOTAL,
		.indom = PM_INDOM_NULL,
		.units = PMI_UNITS(0, 1, 0, 0, PM_TIME_USEC, 0),
		.type = PM_TYPE_U64,
		.sem = PM_SEM_COUNTER,
	},
	[PSI_MEM_FULLAVG] = {
		.pmid = PMID_PSI_MEM_FULLAVG,
		.indom = PMI_INDOM(60, 37),
		.units = PMI_UNITS(0, 0, 0, 0, 0, 0),
		.type = PM_TYPE_FLOAT,
		.sem = PM_SEM_INSTANT,
	},
};

struct act_metrics psi_mem_metrics = {
	.count = PSI_MEM_METRIC_COUNT,
	.names = psi_mem_metric_names,
	.descs = psi_mem_metric_descs,
};

#else	/* undef HAVE_PCP */
void pcp_def_percpu_int_metrics(struct activity *a, int cpu) {}
void pcp_def_pcsw_metrics(struct activity *a) {}
void pcp_def_irq_metrics(struct activity *a) {}
void pcp_def_swap_metrics(struct activity *a) {}
void pcp_def_paging_metrics(struct activity *a) {}
void pcp_def_io_metrics(struct activity *a) {}
void pcp_def_memory_metrics(struct activity *a) {}
void pcp_def_ktables_metrics(struct activity *a) {}
void pcp_def_queue_metrics(struct activity *a) {}
void pcp_def_disk_metrics(struct activity *a) {}
void pcp_def_net_dev_metrics(struct activity *a) {}
void pcp_def_serial_metrics(struct activity *a) {}
void pcp_def_net_nfs_metrics(struct activity *a) {}
void pcp_def_net_nfsd_metrics(struct activity *a) {}
void pcp_def_net_sock_metrics(struct activity *a) {}
void pcp_def_net_ip_metrics(struct activity *a) {}
void pcp_def_net_eip_metrics(struct activity *a) {}
void pcp_def_net_icmp_metrics(struct activity *a) {}
void pcp_def_net_eicmp_metrics(struct activity *a) {}
void pcp_def_net_tcp_metrics(struct activity *a) {}
void pcp_def_net_etcp_metrics(struct activity *a) {}
void pcp_def_net_udp_metrics(struct activity *a) {}
void pcp_def_net_sock6_metrics(struct activity *a) {}
void pcp_def_net_ip6_metrics(struct activity *a) {}
void pcp_def_net_eip6_metrics(struct acivity *a) {}
void pcp_def_net_icmp6_metrics(struct activity *a) {}
void pcp_def_net_eicmp6_metrics(struct activity *a) {}
void pcp_def_net_udp6_metrics(struct activity *a) {}
void pcp_def_huge_metrics(struct activity *a) {}
void pcp_def_pwr_fan_metrics(struct activity *a) {}
void pcp_def_pwr_temp_metrics(struct activity *a) {}
void pcp_def_pwr_in_metrics(struct activity *a) {}
void pcp_def_pwr_bat_metrics(struct activity *a) {}
void pcp_def_pwr_usb_metrics(struct activity *a) {}
void pcp_def_filesystem_metrics(struct activity *a) {}
void pcp_def_fchost_metrics(struct activity *a) {}
void pcp_def_psi_metrics(struct activity *a) {}
#endif /* HAVE_PCP */
