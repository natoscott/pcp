/*
 * pcp_stats.c: Functions used to read and write PCP archives.
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

#include "sa.h"
#include "pcp_stats.h"
#include "pcp_def_metrics.h"

#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

#ifdef HAVE_PCP
#include <pcp/pmapi.h>
#include <pcp/import.h>
#endif

extern struct record_header *record_hdr;
extern struct activity *act[];
extern uint64_t flags;
extern char bat_status[][16];

/*
 ***************************************************************************
 * Extract minimum 32-bit unsigned integer PCP metric value from a valueset.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @inst	Index into metric instances (zero for PM_IN_NULL).
 * @descs	Descriptor array containing matching @metric information.
 * @metric	Lookup key for a given metric in @descs descriptor array.
 ***************************************************************************
 */
unsigned long pcp_read_u32(pmValueSet *values, int inst, pmDesc *descs, int metric)
{
	pmAtomValue atom;

	pmExtractValue(values->valfmt, &values->vlist[inst], descs[metric].type,
			&atom, PM_TYPE_U32);
	return atom.ul;
}

/*
 ***************************************************************************
 * Extract minimum 64-bit unsigned integer PCP metric value from a valueset.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @inst	Index into metric instances (zero for PM_IN_NULL).
 * @descs	Descriptor array containing matching @metric information.
 * @metric	Lookup key for a given metric in @descs descriptor array.
 ***************************************************************************
 */
unsigned long long pcp_read_u64(pmValueSet *values, int inst, pmDesc *descs, int metric)
{
	pmAtomValue atom;

	pmExtractValue(values->valfmt, &values->vlist[inst], descs[metric].type,
			&atom, PM_TYPE_U64);
	return atom.ull;
}

/*
 ***************************************************************************
 * Extract 32-bit floating point PCP metric values from a valueset.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @inst	Index into metric instances (zero for PM_IN_NULL).
 * @descs	Descriptor array containing matching @metric information.
 * @metric	Lookup key for a given metric in @descs descriptor array.
 ***************************************************************************
 */
float pcp_read_float(pmValueSet *values, int inst, pmDesc *descs, int metric)
{
	pmAtomValue atom;

	pmExtractValue(values->valfmt, &values->vlist[inst], descs[metric].type,
			&atom, PM_TYPE_FLOAT);
	return atom.f;
}

/*
 ***************************************************************************
 * Extract 64-bit floating point PCP metric values from a valueset.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @inst	Index into metric instances (zero for PM_IN_NULL).
 * @descs	Descriptor array containing matching @metric information.
 * @metric	Lookup key for a given metric in @descs descriptor array.
 ***************************************************************************
 */
double pcp_read_double(pmValueSet *values, int inst, pmDesc *descs, int metric)
{
	pmAtomValue atom;

	pmExtractValue(values->valfmt, &values->vlist[inst], descs[metric].type,
			&atom, PM_TYPE_DOUBLE);
	return atom.d;
}

/*
 ***************************************************************************
 * Extract character array PCP metric values from a valueset.
 * Caller must free the returned character array (string).
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @inst	Index into metric instances (zero for PM_IN_NULL).
 * @descs	Descriptor array containing matching @metric information.
 * @metric	Lookup key for a given metric in @descs descriptor array.
 ***************************************************************************
 */
char *pcp_read_str(pmValueSet *values, int inst, pmDesc *descs, int metric)
{
	pmAtomValue atom;

	pmExtractValue(values->valfmt, &values->vlist[inst], descs[metric].type,
			&atom, PM_TYPE_STRING);
	return atom.cp;
}

/*
 ***************************************************************************
 * Write CPU statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_cpu_stats(struct activity *a, int curr)
{
	int i;
	unsigned long long deltot_jiffies = 1;
	char buf[64], cpuno[64];
	unsigned char offline_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};
	char *str;
	struct stats_cpu *scc, *scp;

	/*
	 * @nr[curr] cannot normally be greater than @nr_ini.
	 * Yet we have created PCP metrics only for @nr_ini CPU.
	 */
	if (a->nr[curr] > a->nr_ini) {
		a->nr_ini = a->nr[curr];
	}

	/*
	 * Compute CPU "all" as sum of all individual CPU (on SMP machines)
	 * and look for offline CPU.
	 */
	if (a->nr_ini > 1) {
		deltot_jiffies = get_global_cpu_statistics(a, !curr, curr,
							   flags, offline_cpu_bitmap);
	}

	for (i = 0; (i < a->nr_ini) && (i < a->bitmap->b_size + 1); i++) {

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) ||
		    offline_cpu_bitmap[i >> 3] & (1 << (i & 0x07)))
			/* Don't display CPU */
			continue;

		scc = (struct stats_cpu *) ((char *) a->buf[curr]  + i * a->msize);
		scp = (struct stats_cpu *) ((char *) a->buf[!curr] + i * a->msize);

		if (!i) {
			/* This is CPU "all" */
			str = NULL;

			if (a->nr_ini == 1) {
				/*
				 * This is a UP machine. In this case
				 * interval has still not been calculated.
				 */
				deltot_jiffies = get_per_cpu_interval(scc, scp);
			}
			if (!deltot_jiffies) {
				/* CPU "all" cannot be tickless */
				deltot_jiffies = 1;
			}
		}
		else {
			sprintf(cpuno, "cpu%d", i - 1);
			str = cpuno;

			/*
			 * Recalculate interval for current proc.
			 * If result is 0 then current CPU is a tickless one.
			 */
			deltot_jiffies = get_per_cpu_interval(scc, scp);

			if (!deltot_jiffies) {
				/* Current CPU is tickless */
				pmiPutValue("kernel.percpu.cpu.user", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.nice", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.sys", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.iowait", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.steal", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.hardirq", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.softirq", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.guest", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.guest_nice", cpuno, "0");
				pmiPutValue("kernel.percpu.cpu.idle", cpuno, "100");

				continue;
			}
		}

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_user - scc->cpu_guest);
		pmiPutValue(i ? "kernel.percpu.cpu.user" : "kernel.all.cpu.user", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_nice - scc->cpu_guest_nice);
		pmiPutValue(i ? "kernel.percpu.cpu.nice" : "kernel.all.cpu.nice", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_sys);
		pmiPutValue(i ? "kernel.percpu.cpu.sys" : "kernel.all.cpu.sys", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_iowait);
		pmiPutValue(i ? "kernel.percpu.cpu.iowait" : "kernel.all.cpu.iowait", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_steal);
		pmiPutValue(i ? "kernel.percpu.cpu.steal" : "kernel.all.cpu.steal", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_hardirq + scc->cpu_softirq);
		pmiPutValue(i ? "kernel.percpu.cpu.irq.total" : "kernel.all.cpu.irq.total", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_hardirq);
		pmiPutValue(i ? "kernel.percpu.cpu.irq.hard" : "kernel.all.cpu.irq.hard", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_softirq);
		pmiPutValue(i ? "kernel.percpu.cpu.irq.soft" : "kernel.all.cpu.irq.soft", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_guest);
		pmiPutValue(i ? "kernel.percpu.cpu.guest" : "kernel.all.cpu.guest", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_guest_nice);
		pmiPutValue(i ? "kernel.percpu.cpu.guest_nice" : "kernel.all.cpu.guest_nice", str, buf);

		snprintf(buf, sizeof(buf), "%llu", scc->cpu_idle);
		pmiPutValue(i ? "kernel.percpu.cpu.idle" : "kernel.all.cpu.idle", str, buf);
	}
}

/*
 ***************************************************************************
 * Read CPU statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_cpu_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0	// TODO: per-CPU
	switch (values->pmid) {

		case PMID_CPU_ALLCPU_USER:
		case PMID_CPU_ALLCPU_SYS:
		case PMID_CPU_ALLCPU_NICE:
		case PMID_CPU_ALLCPU_IDLE:
		case PMID_CPU_ALLCPU_WAITTOTAL:
		case PMID_CPU_ALLCPU_IRQTOTAL:
		case PMID_CPU_ALLCPU_IRQSOFT:
		case PMID_CPU_ALLCPU_IRQHARD:
		case PMID_CPU_ALLCPU_STEAL:
		case PMID_CPU_ALLCPU_GUEST:
		case PMID_CPU_ALLCPU_GUESTNICE:
		case PMID_CPU_PERCPU_USER:
		case PMID_CPU_PERCPU_NICE:
		case PMID_CPU_PERCPU_SYS:
		case PMID_CPU_PERCPU_IDLE:
		case PMID_CPU_PERCPU_WAITTOTAL:
		case PMID_CPU_PERCPU_IRQTOTAL:
		case PMID_CPU_PERCPU_IRQSOFT:
		case PMID_CPU_PERCPU_IRQHARD:
		case PMID_CPU_PERCPU_STEAL:
		case PMID_CPU_PERCPU_GUEST:
		case PMID_CPU_PERCPU_GUESTNICE:
		case PMID_CPU_PERCPU_INTERRUPTS:
	}
#endif
fprintf(stderr, "%s: not yet implemented\n", __FUNCTION__);
}

/*
 ***************************************************************************
 * Write softnet statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_softnet_stats(struct activity *a, int curr)
{
	int i;
	struct stats_softnet *ssnc;
	char buf[64], cpuno[64];
	unsigned char offline_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};

	/*
	 * @nr[curr] cannot normally be greater than @nr_ini.
	 * Yet we have created PCP metrics only for @nr_ini CPU.
	 */
	if (a->nr[curr] > a->nr_ini) {
		a->nr_ini = a->nr[curr];
	}

	/* Compute statistics for CPU "all" */
	get_global_soft_statistics(a, !curr, curr, flags, offline_cpu_bitmap);

	for (i = 0; (i < a->nr_ini) && (i < a->bitmap->b_size + 1); i++) {

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))) ||
		    offline_cpu_bitmap[i >> 3] & (1 << (i & 0x07)))
			/* No */
			continue;

                ssnc = (struct stats_softnet *) ((char *) a->buf[curr]  + i * a->msize);

		if (!i) {
			/* This is CPU "all" */
			continue;
		}
		else {
			sprintf(cpuno, "cpu%d", i - 1);
		}

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) ssnc->processed);
		pmiPutValue("network.softnet.percpu.processed", cpuno, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) ssnc->dropped);
		pmiPutValue("network.softnet.percpu.dropped", cpuno, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) ssnc->time_squeeze);
		pmiPutValue("network.softnet.percpu.time_squeeze", cpuno, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) ssnc->received_rps);
		pmiPutValue("network.softnet.percpu.received_rps", cpuno, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) ssnc->flow_limit);
		pmiPutValue("network.softnet.percpu.flow_limit", cpuno, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) ssnc->backlog_len);
		pmiPutValue("network.softnet.percpu.backlog_length", cpuno, buf);
	}
}

/*
 ***************************************************************************
 * Read softnet statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_softnet_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0 // TODO: per-CPU
	struct stats_softnet *ssnc;

	switch (values->pmid) {

		case PMID_SOFTNET_ALLCPU_PROCESSED:
		case PMID_SOFTNET_ALLCPU_DROPPED:
		case PMID_SOFTNET_ALLCPU_TIMESQUEEZE:
		case PMID_SOFTNET_ALLCPU_RECEIVEDRPS:
		case PMID_SOFTNET_ALLCPU_FLOWLIMIT:
		case PMID_SOFTNET_ALLCPU_BACKLOGLENGTH:
		case PMID_SOFTNET_PERCPU_PROCESSED:
		case PMID_SOFTNET_PERCPU_DROPPED:
		case PMID_SOFTNET_PERCPU_TIMESQUEEZE:
		case PMID_SOFTNET_PERCPU_RECEIVEDRPS:
		case PMID_SOFTNET_PERCPU_FLOWLIMIT:
		case PMID_SOFTNET_PERCPU_BACKLOGLENGTH:
	}
#endif
fprintf(stderr, "%s: not yet implemented\n", __FUNCTION__);
}

/*
 ***************************************************************************
 * Write task creation and context switch statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pcsw_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_pcsw
		*spc = (struct stats_pcsw *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->context_switch);
	pmiPutValue("kernel.all.pswitch", NULL, buf);

	snprintf(buf, sizeof(buf), "%lu", spc->processes);
	pmiPutValue("kernel.all.sysfork", NULL, buf);
}

/*
 ***************************************************************************
 * Read task creation and context switch statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_pcsw_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_pcsw
		*spc = (struct stats_pcsw *) a->buf[curr];

	switch (values->pmid) {
	case PMID_PCSW_CONTEXT_SWITCH:
		spc->context_switch = pcp_read_u64(values, 0,
						pcsw_metric_descs,
						PCSW_CONTEXT_SWITCH);
		break;

	case PMID_PCSW_FORK_SYSCALLS:
		spc->processes = pcp_read_u64(values, 0,
						pcsw_metric_descs,
						PCSW_FORK_SYSCALLS);
		break;
	}
}

/*
 ***************************************************************************
 * Write interrupts statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_irq_stats(struct activity *a, int curr)
{
	int i, c;
	char buf[64], name[64];
	struct stats_irq *stc_cpu_irq, *stc_cpuall_irq;
	unsigned char masked_cpu_bitmap[BITMAP_SIZE(NR_CPUS)] = {0};

	/* @nr[curr] cannot normally be greater than @nr_ini */
	if (a->nr[curr] > a->nr_ini) {
		a->nr_ini = a->nr[curr];
	}

	/* Identify offline and unselected CPU, and keep persistent statistics values */
	get_global_int_statistics(a, !curr, curr, flags, masked_cpu_bitmap);

	for (i = 0; i < a->nr2; i++) {

		stc_cpuall_irq = (struct stats_irq *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of interrupts has been entered on the command line */
			if (!search_list_item(a->item_list, stc_cpuall_irq->irq_name))
				/* Interrupt not found in list */
				continue;
		}

		for (c = 0; (c < a->nr[curr]) && (c < a->bitmap->b_size + 1); c++) {

			stc_cpu_irq = (struct stats_irq *) ((char *) a->buf[curr] + c * a->msize * a->nr2
										  + i * a->msize);

			/* Should current CPU (including CPU "all") be processed? */
			if (masked_cpu_bitmap[c >> 3] & (1 << (c & 0x07)))
				/* No */
				continue;

			snprintf(buf, sizeof(buf), "%u", stc_cpu_irq->irq_nr);

			if (!c) {
				/* This is CPU "all" */
				if (!i) {
					/* This is interrupt "sum" */
					pmiPutValue("kernel.all.intr", NULL, buf);
				}
				else {
					pmiPutValue("kernel.all.interrupts.total",
						    stc_cpuall_irq->irq_name, buf);
				}
			}
			else {
				/* This is a particular CPU */
				snprintf(name, sizeof(name), "%s::cpu%d",
					 stc_cpuall_irq->irq_name, c - 1);
				name[sizeof(name) - 1] = '\0';

				pmiPutValue("kernel.percpu.interrupts", name, buf);
			}
		}
	}
}

/*
 ***************************************************************************
 * Read interrupts statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_irq_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0 // TODO: per-CPU and per-interrupt-line
	switch (values->pmid) {
		case PMID_IRQ_ALLIRQ_TOTAL:
		case PMID_IRQ_PERIRQ_TOTAL:
			break;
	}
#endif
fprintf(stderr, "%s: not yet implemented\n", __FUNCTION__);
}

/*
 ***************************************************************************
 * Write swapping statistics to PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_swap_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_swap
		*ssc = (struct stats_swap *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%lu", ssc->pswpin);
	pmiPutValue("swap.pagesin", NULL, buf);

	snprintf(buf, sizeof(buf), "%lu", ssc->pswpout);
	pmiPutValue("swap.pagesout", NULL, buf);
}

/*
 ***************************************************************************
 * Read swapping statistics from PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_swap_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_swap
		*ssc =  (struct stats_swap *) a->buf[curr];

	switch (values->pmid) {
	case PMID_SWAP_PAGESIN:
		ssc->pswpin = pcp_read_u32(values, 0, swap_metric_descs,
						SWAP_PAGESIN);
		break;

	case PMID_SWAP_PAGESOUT:
		ssc->pswpout = pcp_read_u32(values, 0, swap_metric_descs,
						SWAP_PAGESOUT);
		break;
	}
}

/*
 ***************************************************************************
 * Write paging statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_paging_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_paging
		*spc = (struct stats_paging *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgpgin);
	pmiPutValue("mem.vmstat.pgpgin", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgpgout);
	pmiPutValue("mem.vmstat.pgpgout", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgfault);
	pmiPutValue("mem.vmstat.pgfault", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgmajfault);
	pmiPutValue("mem.vmstat.pgmajfault", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgfree);
	pmiPutValue("mem.vmstat.pgfree", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgscan_kswapd);
	pmiPutValue("mem.vmstat.pgscan_kswapd_total", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgscan_direct);
	pmiPutValue("mem.vmstat.pgscan_direct_total", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) spc->pgsteal);
	pmiPutValue("mem.vmstat.pgsteal_total", NULL, buf);
}

/*
 ***************************************************************************
 * Read paging statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_paging_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_paging
		*spc = (struct stats_paging *) a->buf[curr];

	switch (values->pmid) {
	case PMID_PAGING_PGPGIN:
		spc->pgpgin = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGPGIN);
			break;

	case PMID_PAGING_PGPGOUT:
		spc->pgpgout = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGPGOUT);
		break;

	case PMID_PAGING_PGFAULT:
		spc->pgfault = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGFAULT);
		break;

	case PMID_PAGING_PGMAJFAULT:
		spc->pgmajfault = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGMAJFAULT);
		break;

	case PMID_PAGING_PGFREE:
		spc->pgfree = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGFREE);
		break;

	case PMID_PAGING_PGSCANDIRECT:
		spc->pgscan_direct = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGSCANDIRECT);
		break;

	case PMID_PAGING_PGSCANKSWAPD:
		spc->pgscan_kswapd = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGSCANKSWAPD);
		break;

	case PMID_PAGING_PGSTEAL:
		spc->pgsteal = pcp_read_u64(values, 0,
						paging_metric_descs,
						PAGING_PGSTEAL);
		break;
	}
}

/*
 ***************************************************************************
 * Write I/O and transfer rate statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_io_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_io
		*sic = (struct stats_io *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", sic->dk_drive);
	pmiPutValue("disk.all.total", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sic->dk_drive_rio);
	pmiPutValue("disk.all.read", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu",sic->dk_drive_wio);
	pmiPutValue("disk.all.write", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sic->dk_drive_dio);
	pmiPutValue("disk.all.discard", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sic->dk_drive_rblk);
	pmiPutValue("disk.all.read_bytes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sic->dk_drive_wblk);
	pmiPutValue("disk.all.write_bytes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sic->dk_drive_dblk);
	pmiPutValue("disk.all.discard_bytes", NULL, buf);
}

/*
 ***************************************************************************
 * Read I/O and transfer rate statistics from PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_io_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_io
		*sic = (struct stats_io *) a->buf[curr];

	switch (values->pmid) {
	case PMID_IO_ALLDEV_TOTAL:
		sic->dk_drive = pcp_read_u64(values, 0, io_metric_descs,
						IO_ALLDEV_TOTAL);
		break;

	case PMID_IO_ALLDEV_READ:
		sic->dk_drive_rio = pcp_read_u64(values, 0, io_metric_descs,
						IO_ALLDEV_READ);
		break;

	case PMID_IO_ALLDEV_WRITE:
		sic->dk_drive_wio = pcp_read_u64(values, 0, io_metric_descs,
						IO_ALLDEV_WRITE);
		break;

	case PMID_IO_ALLDEV_DISCARD:
		sic->dk_drive_dio = pcp_read_u64(values, 0, io_metric_descs,
						IO_ALLDEV_DISCARD);
		break;

	case PMID_IO_ALLDEV_READBYTES:
		sic->dk_drive_rblk = pcp_read_u64(values, 0, io_metric_descs,
						IO_ALLDEV_READBYTES);
		break;

	case PMID_IO_ALLDEV_WRITEBYTES:
		sic->dk_drive_wblk = pcp_read_u64(values, 0, io_metric_descs,
						IO_ALLDEV_WRITEBYTES);
		break;

	case PMID_IO_ALLDEV_DISCARDBYTES:
		sic->dk_drive_dblk = pcp_read_u64(values, 0, io_metric_descs,
						IO_ALLDEV_DISCARDBYTES);
		break;
	}
}

/*
 ***************************************************************************
 * Write memory statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_memory_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_memory
		*smc = (struct stats_memory *) a->buf[curr];

	if (DISPLAY_MEMORY(a->opt_flags)) {

		snprintf(buf, sizeof(buf), "%lu", (unsigned long) (smc->tlmkb >> 10));
		pmiPutValue("hinv.physmem", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->tlmkb);
		pmiPutValue("mem.physmem", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->frmkb);
		pmiPutValue("mem.util.free", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->availablekb);
		pmiPutValue("mem.util.available", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->tlmkb - smc->frmkb);
		pmiPutValue("mem.util.used", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->bufkb);
		pmiPutValue("mem.util.bufmem", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->camkb);
		pmiPutValue("mem.util.cached", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->comkb);
		pmiPutValue("mem.util.committed_AS", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->activekb);
		pmiPutValue("mem.util.active", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->inactkb);
		pmiPutValue("mem.util.inactive", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->dirtykb);
		pmiPutValue("mem.util.dirty", NULL, buf);

		if (DISPLAY_MEM_ALL(a->opt_flags)) {

			snprintf(buf, sizeof(buf), "%llu", smc->anonpgkb);
			pmiPutValue("mem.util.anonpages", NULL, buf);

			snprintf(buf, sizeof(buf), "%llu", smc->slabkb);
			pmiPutValue("mem.util.slab", NULL, buf);

			snprintf(buf, sizeof(buf), "%llu", smc->kstackkb);
			pmiPutValue("mem.util.kernelStack", NULL, buf);

			snprintf(buf, sizeof(buf), "%llu", smc->pgtblkb);
			pmiPutValue("mem.util.pageTables", NULL, buf);

			snprintf(buf, sizeof(buf), "%llu", smc->vmusedkb);
			pmiPutValue("mem.util.vmallocUsed", NULL, buf);
		}
	}

	if (DISPLAY_SWAP(a->opt_flags)) {

		snprintf(buf, sizeof(buf), "%llu", smc->frskb);
		pmiPutValue("mem.util.swapFree", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->tlskb);
		pmiPutValue("mem.util.swapTotal", NULL, buf);

		snprintf(buf, sizeof(buf), "%llu", smc->caskb);
		pmiPutValue("mem.util.swapCached", NULL, buf);
	}
}

/*
 ***************************************************************************
 * Read memory statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_memory_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_memory
		*smc = (struct stats_memory *) a->buf[curr];

	switch (values->pmid) {
	case PMID_MEM_PHYS_KB:
		smc->tlmkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_PHYS_KB);
		break;

	case PMID_MEM_UTIL_FREE:
		smc->frmkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_FREE);
		break;

	case PMID_MEM_UTIL_AVAIL:
		smc->availablekb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_AVAIL);
		break;

	case PMID_MEM_UTIL_BUFFER:
		smc->bufkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_BUFFER);
		break;

	case PMID_MEM_UTIL_CACHED:
		smc->camkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_CACHED);
		break;

	case PMID_MEM_UTIL_COMMITAS:
		smc->comkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_COMMITAS);
		break;

	case PMID_MEM_UTIL_ACTIVE:
		smc->activekb = pcp_read_u64(values, 0, mem_metric_descs,
					MEM_UTIL_ACTIVE);
		break;

	case PMID_MEM_UTIL_INACTIVE:
		smc->inactkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_INACTIVE);
		break;

	case PMID_MEM_UTIL_DIRTY:
		smc->dirtykb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_DIRTY);
		break;

	case PMID_MEM_UTIL_ANON:
		smc->anonpgkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_ANON);
		break;

	case PMID_MEM_UTIL_SLAB:
		smc->slabkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_SLAB);
		break;

	case PMID_MEM_UTIL_KSTACK:
		smc->kstackkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_KSTACK);
		break;

	case PMID_MEM_UTIL_PGTABLE:
		smc->pgtblkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_PGTABLE);
		break;

	case PMID_MEM_UTIL_VMALLOC:
		smc->vmusedkb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_VMALLOC);
		break;

	case PMID_MEM_UTIL_SWAPFREE:
		smc->frskb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_SWAPFREE);
		break;

	case PMID_MEM_UTIL_SWAPTOTAL:
		smc->tlskb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_SWAPTOTAL);
		break;

	case PMID_MEM_UTIL_SWAPCACHED:
		smc->caskb = pcp_read_u64(values, 0, mem_metric_descs,
						MEM_UTIL_SWAPCACHED);
		break;
	}
}

/*
 ***************************************************************************
 * Write kernel tables statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_ktables_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_ktables
		*skc = (struct stats_ktables *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%lu", (unsigned long) skc->dentry_stat);
	pmiPutValue("vfs.dentry.count", NULL, buf);

	snprintf(buf, sizeof(buf), "%lu", (unsigned long) skc->file_used);
	pmiPutValue("vfs.files.count", NULL, buf);

	snprintf(buf, sizeof(buf), "%lu", (unsigned long) skc->inode_used);
	pmiPutValue("vfs.inodes.count", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", skc->pty_nr);
	pmiPutValue("kernel.all.nptys", NULL, buf);
}

/*
 ***************************************************************************
 * Read kernel tables statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_ktable_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_ktables
		*skc = (struct stats_ktables *) a->buf[curr];

	switch (values->pmid) {
	case PMID_KTABLE_DENTRYS:
		skc->dentry_stat = pcp_read_u32(values, 0, ktable_metric_descs,
						KTABLE_DENTRYS);
		break;

	case PMID_KTABLE_FILES:
		skc->file_used = pcp_read_u32(values, 0, ktable_metric_descs,
						KTABLE_FILES);
		break;

	case PMID_KTABLE_INODES:
		skc->inode_used = pcp_read_u32(values, 0, ktable_metric_descs,
						KTABLE_INODES);
		break;

	case PMID_KTABLE_PTYS:
		skc->pty_nr = pcp_read_u32(values, 0, ktable_metric_descs,
						KTABLE_PTYS);
		break;
	}
}

/*
 ***************************************************************************
 * Write queue and load statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_queue_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_queue
		*sqc = (struct stats_queue *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%lu", (unsigned long) sqc->nr_running);
	pmiPutValue("kernel.all.runnable", NULL, buf);

	snprintf(buf, sizeof(buf), "%lu", (unsigned long) sqc->nr_threads);
	pmiPutValue("kernel.all.nprocs", NULL, buf);

	snprintf(buf, sizeof(buf), "%lu", (unsigned long) sqc->procs_blocked);
	pmiPutValue("kernel.all.blocked", NULL, buf);

	snprintf(buf, sizeof(buf), "%f", (double) sqc->load_avg_1 / 100);
	pmiPutValue("kernel.all.load", "1 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) sqc->load_avg_5 / 100);
	pmiPutValue("kernel.all.load", "5 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) sqc->load_avg_15 / 100);
	pmiPutValue("kernel.all.load", "15 minute", buf);
}

/*
 ***************************************************************************
 * Read queue and load statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_kqueue_stats(pmValueSet *values, struct activity *a, int curr)
{
	int i;
	struct stats_queue
		*sqc = (struct stats_queue *) a->buf[curr];

	switch (values->pmid) {

	case PMID_KQUEUE_RUNNABLE:
		sqc->nr_running = pcp_read_u32(values, 0,
						kqueue_metric_descs,
						KQUEUE_RUNNABLE);
		break;

	case PMID_KQUEUE_PROCESSES:
		sqc->nr_threads = pcp_read_u32(values, 0,
						kqueue_metric_descs,
						KQUEUE_PROCESSES);
		break;

	case PMID_KQUEUE_BLOCKED:
		sqc->procs_blocked = pcp_read_u32(values, 0,
						kqueue_metric_descs,
						KQUEUE_BLOCKED);
		break;

	case PMID_KQUEUE_LOADAVG:
		for (i = 0; i < values->numval; i++) {
			switch (values->vlist[i].inst) {

			case 1: /* 1 minute average */
				sqc->load_avg_1 = 100 * (unsigned int)
					pcp_read_float(values, i,
						kqueue_metric_descs,
						KQUEUE_LOADAVG);
				break;
			case 5: /* 5 minute average */
				sqc->load_avg_5 = 100 * (unsigned int)
					pcp_read_float(values, i,
						kqueue_metric_descs,
						KQUEUE_LOADAVG);
				break;
			case 15: /* 15 minute average */
				sqc->load_avg_15 = 100 * (unsigned int)
					pcp_read_float(values, i,
						kqueue_metric_descs,
						KQUEUE_LOADAVG);
				break;
			}
		}
		break;
	}
}

/*
 ***************************************************************************
 * Write disks statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_disk_stats(struct activity *a, int curr)
{
	int i;
	struct stats_disk *sdc;
	char *dev_name;
	char buf[64];

	for (i = 0; i < a->nr[curr]; i++) {

		sdc = (struct stats_disk *) ((char *) a->buf[curr] + i * a->msize);

		/* Get device name */
		dev_name = get_device_name(sdc->major, sdc->minor, sdc->wwn, sdc->part_nr,
					   DISPLAY_PRETTY(flags), DISPLAY_PERSIST_NAME_S(flags),
					   USE_STABLE_ID(flags), NULL);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, dev_name))
				/* Device not found */
				continue;
		}

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sdc->nr_ios);
		pmiPutValue("disk.dev.total", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) (sdc->rd_sect + sdc->wr_sect) / 2);
		pmiPutValue("disk.dev.total_bytes", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sdc->rd_sect / 2);
		pmiPutValue("disk.dev.read_bytes", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sdc->wr_sect / 2);
		pmiPutValue("disk.dev.write_bytes", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sdc->dc_sect / 2);
		pmiPutValue("disk.dev.discard_bytes", dev_name, buf);

		snprintf(buf, sizeof(buf), "%lu", (unsigned long) sdc->rd_ticks + sdc->wr_ticks);
		pmiPutValue("disk.dev.total_rawactive", dev_name, buf);

		snprintf(buf, sizeof(buf), "%lu", (unsigned long) sdc->rd_ticks);
		pmiPutValue("disk.dev.read_rawactive", dev_name, buf);

		snprintf(buf, sizeof(buf), "%lu", (unsigned long) sdc->wr_ticks);
		pmiPutValue("disk.dev.write_rawactive", dev_name, buf);

		snprintf(buf, sizeof(buf), "%lu", (unsigned long)sdc->dc_ticks);
		pmiPutValue("disk.dev.discard_rawactive", dev_name, buf);

		snprintf(buf, sizeof(buf), "%lu", (unsigned long)sdc->tot_ticks);
		pmiPutValue("disk.dev.avactive", dev_name, buf);

		snprintf(buf, sizeof(buf), "%lu", (unsigned long)sdc->rq_ticks);
		pmiPutValue("disk.dev.aveq", dev_name, buf);
	}
}

/*
 ***************************************************************************
 * Read disks statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_disk_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0 // TODO: per-disk
	switch (values->pmid) {
		case PMID_DISK_PERDEV_READ:
		case PMID_DISK_PERDEV_WRITE:
		case PMID_DISK_PERDEV_TOTAL:
		case PMID_DISK_PERDEV_TOTALBYTES:
		case PMID_DISK_PERDEV_READBYTES:
		case PMID_DISK_PERDEV_WRITEBYTES:
		case PMID_DISK_PERDEV_DISCARDBYTES:
		case PMID_DISK_PERDEV_READACTIVE:
		case PMID_DISK_PERDEV_WRITEACTIVE:
		case PMID_DISK_PERDEV_TOTALACTIVE:
		case PMID_DISK_PERDEV_DISCARDACTIVE:
		case PMID_DISK_PERDEV_AVACTIVE:
		case PMID_DISK_PERDEV_AVQUEUE:
			break;
	}
#endif
fprintf(stderr, "%s: not yet implemented\n", __FUNCTION__);
}

/*
 ***************************************************************************
 * Write network interfaces statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_dev_stats(struct activity *a, int curr)
{
	int i;
	struct stats_net_dev *sndc;
	char buf[64];

	for (i = 0; i < a->nr[curr]; i++) {

		sndc = (struct stats_net_dev *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, sndc->interface))
				/* Device not found */
				continue;
		}

		/*
		 * No need to look for the previous sample values: PCP displays the raw
		 * counter value, not its variation over the interval.
		 * The whole list of network interfaces present in file has been created
		 * (this is goal of the FO_ITEM_LIST option set for pcp_fmt report format -
		 * see format.c). So no need to wonder if an instance needs to be created
		 * for current interface.
		 */

		snprintf(buf, sizeof(buf), "%llu", sndc->rx_packets);
		pmiPutValue("network.interface.in.packets", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", sndc->tx_packets);
		pmiPutValue("network.interface.out.packets", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", sndc->rx_bytes);
		pmiPutValue("network.interface.in.bytes", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", sndc->tx_bytes);
		pmiPutValue("network.interface.out.bytes", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", sndc->rx_compressed);
		pmiPutValue("network.interface.in.compressed", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", sndc->tx_compressed);
		pmiPutValue("network.interface.out.compressed", sndc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", sndc->multicast);
		pmiPutValue("network.interface.in.mcasts", sndc->interface, buf);
	}
}

/*
 ***************************************************************************
 * Read network interfaces statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_netdev_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0 // TODO: per-disk
	struct stats_net_dev	*sndc;
	pmInDom			indom = netdev_metric_descs[0].indom;
	char			*name;
	int			i, n, sts, inst;

	for (i = 0; i < values->numval; i++) {
		/* TODO: nuh, probably need to be smarter here */
		inst = values->vlist[i].inst;
		while (inst >= a->nr_allocated[curr]) {
			reallocate_buffer(a);
		}

		/* TODO: optimize this, only needs to be done once */
		sts = pmNameInDom(indom, inst, &name);
		if (sts < 0) {
			/* TODO: error handling? warning? (no instance name) */
			continue;
		}

		sndc = (struct stats_net_dev *) ((char *) a->_buf0 + i * a->msize);
		strncpy(sndc->interface, sizeof(sndc->interface)-1, name);
		sndc->interface[sizeof(sndc->interface)-1] = '\0';
		free(name);

		switch (values->pmid) {
		case PMID_NET_PERINTF_INPACKETS:
			sndc->rx_packets = pcp_read_u64(values, i,
						netdev_metric_descs,
						NET_PERINTF_INPACKETS);
			break;
		case PMID_NET_PERINTF_OUTPACKETS:
			sndc->tx_packets = pcp_read_u64(values, i,
						netdev_metric_descs,
						NET_PERINTF_OUTPACKETS);
			break;
		case PMID_NET_PERINTF_INBYTES:
			sndc->rx_bytes = pcp_read_u64(values, i,
						netdev_metric_descs,
						NET_PERINTF_INBYTES);
			break;
		case PMID_NET_PERINTF_OUTBYTES:
			sndc->tx_bytes = pcp_read_u64(values, i,
						netdev_metric_descs,
						NET_PERINTF_OUTBYTES);
			break;
		case PMID_NET_PERINTF_INCOMPRESS:
			sndc->rx_compressed = pcp_read_u64(values, i,
						netdev_metric_descs,
						NET_PERINTF_INCOMPRESS);
			break;
		case PMID_NET_PERINTF_OUTCOMPRESS:
			sndc->tx_compressed = pcp_read_u64(values, i,
						netdev_metric_descs,
						NET_PERINTF_OUTCOMPRESS);
			break;
		case PMID_NET_PERINTF_INMULTICAST:
			sndc->multicast = pcp_read_u64(values, i,
						netdev_metric_descs,
						NET_PERINTF_INMULTICAST);
			break;
		}
	}
#endif
fprintf(stderr, "%s: not yet implemented\n", __FUNCTION__);
}

/*
 ***************************************************************************
 * Write network interfaces errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_edev_stats(struct activity *a, int curr)
{
	int i;
	struct stats_net_edev *snedc;
	char buf[64];

	for (i = 0; i < a->nr[curr]; i++) {

		snedc = (struct stats_net_edev *) ((char *) a->buf[curr] + i * a->msize);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, snedc->interface))
				/* Device not found */
				continue;
		}

		snprintf(buf, sizeof(buf), "%llu", snedc->rx_errors);
		pmiPutValue("network.interface.in.errors", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->tx_errors);
		pmiPutValue("network.interface.out.errors", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->collisions);
		pmiPutValue("network.interface.collisions", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->rx_dropped);
		pmiPutValue("network.interface.in.drops", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->tx_dropped);
		pmiPutValue("network.interface.out.drops", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->tx_carrier_errors);
		pmiPutValue("network.interface.out.carrier", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->rx_frame_errors);
		pmiPutValue("network.interface.in.frame", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->rx_fifo_errors);
		pmiPutValue("network.interface.in.fifo", snedc->interface, buf);

		snprintf(buf, sizeof(buf), "%llu", snedc->tx_fifo_errors);
		pmiPutValue("network.interface.out.fifo", snedc->interface, buf);
	}
}

/*
 ***************************************************************************
 * Read network interfaces errors statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_enetdev_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0 // TODO: per-interface
	switch (values->pmid) {
		case PMID_NET_EPERINTF_INERRORS:
		case PMID_NET_EPERINTF_OUTERRORS:
		case PMID_NET_EPERINTF_COLLISIONS:
		case PMID_NET_EPERINTF_INDROPS:
		case PMID_NET_EPERINTF_OUTDROPS:
		case PMID_NET_EPERINTF_OUTCARRIER:
		case PMID_NET_EPERINTF_INFRAME:
		case PMID_NET_EPERINTF_INFIFO:
		case PMID_NET_EPERINTF_OUTFIFO:
			break;
	}
#endif
fprintf(stderr, "%s: not yet implemented\n", __FUNCTION__);
}

/*
 ***************************************************************************
 * Write serial lines statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_serial_stats(struct activity *a, int curr)
{
	int i;
	char buf[64], serialno[64];
	struct stats_serial *ssc;

	for (i = 0; i < a->nr[curr]; i++) {

		ssc = (struct stats_serial *) ((char *) a->buf[curr] + i * a->msize);

		snprintf(serialno, sizeof(serialno), "serial%u", ssc->line);

		snprintf(buf, sizeof(buf), "%u", ssc->rx);
		pmiPutValue("tty.serial.rx", serialno, buf);

		snprintf(buf, sizeof(buf), "%u", ssc->tx);
		pmiPutValue("tty.serial.tx", serialno, buf);

		snprintf(buf, sizeof(buf), "%u", ssc->frame);
		pmiPutValue("tty.serial.frame", serialno, buf);

		snprintf(buf, sizeof(buf), "%u", ssc->parity);
		pmiPutValue("tty.serial.parity", serialno, buf);

		snprintf(buf, sizeof(buf), "%u", ssc->brk);
		pmiPutValue("tty.serial.brk", serialno, buf);

		snprintf(buf, sizeof(buf), "%u", ssc->overrun);
		pmiPutValue("tty.serial.overrun", serialno, buf);
	}
}

/*
 ***************************************************************************
 * Read serial lines statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_serial_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0 // TODO: per-serial-line
	switch (values->pmid) {
		case PMID_SERIAL_PERTTY_RX:
		case PMID_SERIAL_PERTTY_TX:
		case PMID_SERIAL_PERTTY_FRAME:
		case PMID_SERIAL_PERTTY_PARITY:
		case PMID_SERIAL_PERTTY_BRK:
		case PMID_SERIAL_PERTTY_OVERRUN:
			break;
	}
#endif
fprintf(stderr, "%s: not yet implemented\n", __FUNCTION__);
}

/*
 ***************************************************************************
 * Write NFS client statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_nfs_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_nfs
		*snnc = (struct stats_net_nfs *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%u", snnc->nfs_rpccnt);
	pmiPutValue("rpc.client.rpccnt", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snnc->nfs_rpcretrans);
	pmiPutValue("rpc.client.rpcretrans", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snnc->nfs_readcnt);
	pmiPutValue("nfs.client.reqs", "read", buf);

	snprintf(buf, sizeof(buf), "%u", snnc->nfs_writecnt);
	pmiPutValue("nfs.client.reqs", "write", buf);

	snprintf(buf, sizeof(buf), "%u", snnc->nfs_accesscnt);
	pmiPutValue("nfs.client.reqs", "access", buf);

	snprintf(buf, sizeof(buf), "%u", snnc->nfs_getattcnt);
	pmiPutValue("nfs.client.reqs", "getattr", buf);
}

/*
 ***************************************************************************
 * Read NFS client statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_nfs_stats(pmValueSet *values, struct activity *a, int curr)
{
	int i;
	struct stats_net_nfs
		*snnc = (struct stats_net_nfs *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NFSCLIENT_RPCCCNT:
		snnc->nfs_rpccnt = pcp_read_u32(values, 0,
						nfsclient_metric_descs,
						NFSCLIENT_RPCCCNT);
		break;

	case PMID_NFSCLIENT_RPCRETRANS:
		snnc->nfs_rpcretrans = pcp_read_u32(values, 0,
						nfsclient_metric_descs,
						NFSCLIENT_RPCRETRANS);
		break;

	case PMID_NFSCLIENT_REQUESTS:
		for (i = 0; i < values->numval; i++) {
			switch (values->vlist[i].inst) {

			case NFS_REQUEST_READ:
				snnc->nfs_readcnt = pcp_read_u32(values, i,
						nfsclient_metric_descs,
						NFSCLIENT_RPCRETRANS);
				break;

			case NFS_REQUEST_WRITE:
				snnc->nfs_writecnt = pcp_read_u32(values, i,
						nfsclient_metric_descs,
						NFSCLIENT_RPCRETRANS);
				break;

			case NFS_REQUEST_ACCESS:
				snnc->nfs_accesscnt = pcp_read_u32(values, i,
						nfsclient_metric_descs,
						NFSCLIENT_RPCRETRANS);
				break;

			case NFS_REQUEST_GETATTR:
				snnc->nfs_getattcnt = pcp_read_u32(values, i,
						nfsclient_metric_descs,
						NFSCLIENT_RPCRETRANS);
				break;
			}
		}
		break;
	}
}

/*
 ***************************************************************************
 * Write NFS server statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_nfsd_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_nfsd
		*snndc = (struct stats_net_nfsd *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_rpccnt);
	pmiPutValue("rpc.server.rpccnt", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_rpcbad);
	pmiPutValue("rpc.server.rpcbadclnt", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_netcnt);
	pmiPutValue("rpc.server.netcnt", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_netudpcnt);
	pmiPutValue("rpc.server.netudpcnt", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_nettcpcnt);
	pmiPutValue("rpc.server.nettcpcnt", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_rchits);
	pmiPutValue("rpc.server.rchits", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_rcmisses);
	pmiPutValue("rpc.server.rcmisses", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_readcnt);
	pmiPutValue("nfs.server.reqs", "read", buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_writecnt);
	pmiPutValue("nfs.server.reqs", "write", buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_accesscnt);
	pmiPutValue("nfs.server.reqs", "access", buf);

	snprintf(buf, sizeof(buf), "%u", snndc->nfsd_getattcnt);
	pmiPutValue("nfs.server.reqs", "getattr", buf);
}

/*
 ***************************************************************************
 * Read NFS server statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_nfsd_stats(pmValueSet *values, struct activity *a, int curr)
{
	int i;
	struct stats_net_nfsd
		*snndc = (struct stats_net_nfsd *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NFSSERVER_RPCCNT:
		snndc->nfsd_rpccnt = pcp_read_u32(values, 0,
						nfsserver_metric_descs,
						NFSSERVER_RPCCNT);
		break;

	case PMID_NFSSERVER_RPCBADCLNT:
		snndc->nfsd_rpcbad = pcp_read_u32(values, 0,
						nfsserver_metric_descs,
						NFSSERVER_RPCBADCLNT);
		break;

	case PMID_NFSSERVER_NETCNT:
		snndc->nfsd_netcnt = pcp_read_u32(values, 0,
						nfsserver_metric_descs,
						NFSSERVER_NETCNT);
		break;

	case PMID_NFSSERVER_NETUDPCNT:
		snndc->nfsd_netudpcnt = pcp_read_u32(values, 0,
						nfsserver_metric_descs,
						NFSSERVER_NETUDPCNT);
		break;

	case PMID_NFSSERVER_NETTCPCNT:
		snndc->nfsd_nettcpcnt = pcp_read_u32(values, 0,
						nfsserver_metric_descs,
						NFSSERVER_NETTCPCNT);
		break;

	case PMID_NFSSERVER_RCHITS:
		snndc->nfsd_rchits = pcp_read_u32(values, 0,
						nfsserver_metric_descs,
						NFSSERVER_RCHITS);
		break;

	case PMID_NFSSERVER_RCMISSES:
		snndc->nfsd_rcmisses = pcp_read_u32(values, 0,
						nfsserver_metric_descs,
						NFSSERVER_RCMISSES);
		break;

	case PMID_NFSSERVER_REQUESTS:
		for (i = 0; i < values->numval; i++) {
		       	switch (values->vlist[i].inst) {

			case NFS_REQUEST_READ:
				snndc->nfsd_readcnt = pcp_read_u32(values, i,
						nfsserver_metric_descs,
						NFSSERVER_REQUESTS);
				break;

			case NFS_REQUEST_WRITE:
				snndc->nfsd_writecnt = pcp_read_u32(values, i,
						nfsserver_metric_descs,
						NFSSERVER_REQUESTS);
				break;

			case NFS_REQUEST_ACCESS:
				snndc->nfsd_accesscnt = pcp_read_u32(values, i,
						nfsserver_metric_descs,
						NFSSERVER_REQUESTS);
				break;

			case NFS_REQUEST_GETATTR:
				snndc->nfsd_getattcnt = pcp_read_u32(values, i,
						nfsserver_metric_descs,
						NFSSERVER_REQUESTS);
				break;
			}
		}
		break;
	}
}

/*
 ***************************************************************************
 * Write network sockets statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_sock_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_sock
		*snsc = (struct stats_net_sock *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%u", snsc->sock_inuse);
	pmiPutValue("network.sockstat.total", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->tcp_inuse);
	pmiPutValue("network.sockstat.tcp.inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->udp_inuse);
	pmiPutValue("network.sockstat.udp.inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->raw_inuse);
	pmiPutValue("network.sockstat.raw.inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->frag_inuse);
	pmiPutValue("network.sockstat.frag.inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->tcp_tw);
	pmiPutValue("network.sockstat.tcp.tw", NULL, buf);
}

/*
 ***************************************************************************
 * Read network sockets statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_sock_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_sock
		*snsc = (struct stats_net_sock *) a->buf[curr];

	switch (values->pmid) {

	case PMID_SOCKET_TOTAL:
		snsc->sock_inuse = pcp_read_u64(values, 0, socket_metric_descs,
						SOCKET_TOTAL);
		break;

	case PMID_SOCKET_TCPINUSE:
		snsc->tcp_inuse = pcp_read_u64(values, 0, socket_metric_descs,
						SOCKET_TCPINUSE);
		break;

	case PMID_SOCKET_UDPINUSE:
		snsc->udp_inuse = pcp_read_u64(values, 0, socket_metric_descs,
						SOCKET_UDPINUSE);
		break;

	case PMID_SOCKET_RAWINUSE:
		snsc->raw_inuse = pcp_read_u64(values, 0, socket_metric_descs,
						SOCKET_RAWINUSE);
		break;

	case PMID_SOCKET_FRAGINUSE:
		snsc->frag_inuse = pcp_read_u64(values, 0, socket_metric_descs,
						SOCKET_FRAGINUSE);
		break;

	case PMID_SOCKET_TCPTW:
		snsc->tcp_tw = pcp_read_u64(values, 0, socket_metric_descs,
						SOCKET_TCPTW);
		break;
	}
}

/*
 ***************************************************************************
 * Write IP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_ip_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_ip
		*snic = (struct stats_net_ip *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", snic->InReceives);
	pmiPutValue("network.ip.inreceives", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->ForwDatagrams);
	pmiPutValue("network.ip.forwdatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->InDelivers);
	pmiPutValue("network.ip.indelivers", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->OutRequests);
	pmiPutValue("network.ip.outrequests", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->ReasmReqds);
	pmiPutValue("network.ip.reasmreqds", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->ReasmOKs);
	pmiPutValue("network.ip.reasmoks", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->FragOKs);
	pmiPutValue("network.ip.fragoks", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->FragCreates);
	pmiPutValue("network.ip.fragcreates", NULL, buf);
}

/*
 ***************************************************************************
 * Read IP network statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_ip_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_ip
		*snic = (struct stats_net_ip *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_IP_INRECEIVES:
		snic->InReceives = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_INRECEIVES);
		break;

	case PMID_NET_IP_FORWDATAGRAMS:
		snic->ForwDatagrams = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_FORWDATAGRAMS);
		break;

	case PMID_NET_IP_INDELIVERS:
		snic->InDelivers = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_INDELIVERS);
		break;

	case PMID_NET_IP_OUTREQUESTS:
		snic->OutRequests = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_OUTREQUESTS);
		break;

	case PMID_NET_IP_REASMREQDS:
		snic->ReasmReqds = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_REASMREQDS);
		break;

	case PMID_NET_IP_REASMOKS:
		snic->ReasmOKs = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_REASMOKS);
		break;

	case PMID_NET_IP_FRAGOKS:
		snic->FragOKs = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_FRAGOKS);
		break;

	case PMID_NET_IP_FRAGCREATES:
		snic->FragCreates = pcp_read_u64(values, 0,
						net_ip_metric_descs,
						NET_IP_FRAGCREATES);
		break;
	}
}
/*
 ***************************************************************************
 * Write IP network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eip_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_eip
		*sneic = (struct stats_net_eip *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", sneic->InHdrErrors);
	pmiPutValue("network.ip.inhdrerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InAddrErrors);
	pmiPutValue("network.ip.inaddrerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InUnknownProtos);
	pmiPutValue("network.ip.inunknownprotos", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InDiscards);
	pmiPutValue("network.ip.indiscards", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->OutDiscards);
	pmiPutValue("network.ip.outdiscards", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->OutNoRoutes);
	pmiPutValue("network.ip.outnoroutes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->ReasmFails);
	pmiPutValue("network.ip.reasmfails", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->FragFails);
	pmiPutValue("network.ip.fragfails", NULL, buf);
}

/*
 ***************************************************************************
 * Read IP network errors statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_eip_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_eip
		*sneic = (struct stats_net_eip *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_EIP_INHDRERRORS:
		sneic->InHdrErrors = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_INHDRERRORS);
		break;

	case PMID_NET_EIP_INADDRERRORS:
		sneic->InAddrErrors = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_INADDRERRORS);
		break;

	case PMID_NET_EIP_INUNKNOWNPROTOS:
		sneic->InUnknownProtos = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_INUNKNOWNPROTOS);
		break;

	case PMID_NET_EIP_INDISCARDS:
		sneic->InDiscards = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_INDISCARDS);
		break;

	case PMID_NET_EIP_OUTDISCARDS:
		sneic->OutDiscards = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_OUTDISCARDS);
		break;

	case PMID_NET_EIP_OUTNOROUTES:
		sneic->OutNoRoutes = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_OUTNOROUTES);
		break;

	case PMID_NET_EIP_REASMFAILS:
		sneic->ReasmFails = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_REASMFAILS);
		break;

	case PMID_NET_EIP_FRAGFAILS:
		sneic->FragFails = pcp_read_u64(values, 0,
						net_eip_metric_descs,
						NET_EIP_FRAGFAILS);
		break;
	}
}

/*
 ***************************************************************************
 * Write ICMP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_icmp_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_icmp
		*snic = (struct stats_net_icmp *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InMsgs);
	pmiPutValue("network.icmp.inmsgs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutMsgs);
	pmiPutValue("network.icmp.outmsgs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InEchos);
	pmiPutValue("network.icmp.inechos", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InEchoReps);
	pmiPutValue("network.icmp.inechoreps", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutEchos);
	pmiPutValue("network.icmp.outechos", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutEchoReps);
	pmiPutValue("network.icmp.outechoreps", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InTimestamps);
	pmiPutValue("network.icmp.intimestamps", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InTimestampReps);
	pmiPutValue("network.icmp.intimestampreps", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutTimestamps);
	pmiPutValue("network.icmp.outtimestamps", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutTimestampReps);
	pmiPutValue("network.icmp.outtimestampreps", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InAddrMasks);
	pmiPutValue("network.icmp.inaddrmasks", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InAddrMaskReps);
	pmiPutValue("network.icmp.inaddrmaskreps", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutAddrMasks);
	pmiPutValue("network.icmp.outaddrmasks", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutAddrMaskReps);
	pmiPutValue("network.icmp.outaddrmaskreps", NULL, buf);
}

/*
 ***************************************************************************
 * Read ICMP network statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_icmp_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_icmp
		*snic = (struct stats_net_icmp *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_ICMP_INMSGS:
		snic->InMsgs = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_INMSGS);
		break;

	case PMID_NET_ICMP_OUTMSGS:
		snic->OutMsgs = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_OUTMSGS);
		break;

	case PMID_NET_ICMP_INECHOS:
		snic->InEchos = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_INECHOS);
		break;

	case PMID_NET_ICMP_INECHOREPS:
		snic->InEchoReps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_INECHOREPS);
		break;

	case PMID_NET_ICMP_OUTECHOS:
		snic->OutEchos = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_OUTECHOS);
		break;

	case PMID_NET_ICMP_OUTECHOREPS:
		snic->OutEchoReps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_OUTECHOREPS);
		break;

	case PMID_NET_ICMP_INTIMESTAMPS:
		snic->InTimestamps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_INTIMESTAMPS);
		break;

	case PMID_NET_ICMP_INTIMESTAMPREPS:
		snic->InTimestampReps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_INTIMESTAMPREPS);
		break;

	case PMID_NET_ICMP_OUTTIMESTAMPS:
		snic->OutTimestamps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_OUTTIMESTAMPS);
		break;

	case PMID_NET_ICMP_OUTTIMESTAMPREPS:
		snic->OutTimestampReps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_OUTTIMESTAMPREPS);
		break;

	case PMID_NET_ICMP_INADDRMASKS:
		snic->InAddrMasks = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_INADDRMASKS);
		break;

	case PMID_NET_ICMP_INADDRMASKREPS:
		snic->InAddrMaskReps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_INADDRMASKREPS);
		break;

	case PMID_NET_ICMP_OUTADDRMASKS:
		snic->OutAddrMasks = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_OUTADDRMASKS);
		break;

	case PMID_NET_ICMP_OUTADDRMASKREPS:
		snic->OutAddrMaskReps = pcp_read_u64(values, 0,
						net_icmp_metric_descs,
						NET_ICMP_OUTADDRMASKREPS);
		break;
	}
}

/*
 ***************************************************************************
 * Write ICMP network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eicmp_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_eicmp
		*sneic = (struct stats_net_eicmp *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InErrors);
	pmiPutValue("network.icmp.inerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutErrors);
	pmiPutValue("network.icmp.outerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InDestUnreachs);
	pmiPutValue("network.icmp.indestunreachs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutDestUnreachs);
	pmiPutValue("network.icmp.outdestunreachs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InTimeExcds);
	pmiPutValue("network.icmp.intimeexcds", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutTimeExcds);
	pmiPutValue("network.icmp.outtimeexcds", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InParmProbs);
	pmiPutValue("network.icmp.inparmprobs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutParmProbs);
	pmiPutValue("network.icmp.outparmprobs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InSrcQuenchs);
	pmiPutValue("network.icmp.insrcquenchs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutSrcQuenchs);
	pmiPutValue("network.icmp.outsrcquenchs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InRedirects);
	pmiPutValue("network.icmp.inredirects", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutRedirects);
	pmiPutValue("network.icmp.outredirects", NULL, buf);
}

/*
 ***************************************************************************
 * Read ICMP network errors statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_eicmp_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_eicmp
		*sneic = (struct stats_net_eicmp *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_EICMP_INERRORS:
		sneic->InErrors = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_INERRORS);
		break;

	case PMID_NET_EICMP_OUTERRORS:
		sneic->OutErrors = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_OUTERRORS);
		break;

	case PMID_NET_EICMP_INDESTUNREACHS:
		sneic->InDestUnreachs = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_INDESTUNREACHS);
		break;

	case PMID_NET_EICMP_OUTDESTUNREACHS:
		sneic->OutDestUnreachs = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_OUTDESTUNREACHS);
		break;

	case PMID_NET_EICMP_INTIMEEXCDS:
		sneic->InTimeExcds = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_INTIMEEXCDS);
		break;

	case PMID_NET_EICMP_OUTTIMEEXCDS:
		sneic->OutTimeExcds = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_OUTTIMEEXCDS);
		break;

	case PMID_NET_EICMP_INPARMPROBS:
		sneic->InParmProbs = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_INPARMPROBS);
		break;

	case PMID_NET_EICMP_OUTPARMPROBS:
		 sneic->OutParmProbs = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_OUTPARMPROBS);
		break;

	case PMID_NET_EICMP_INSRCQUENCHS:
		sneic->InSrcQuenchs = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_INSRCQUENCHS);
		break;

	case PMID_NET_EICMP_OUTSRCQUENCHS:
		sneic->OutSrcQuenchs = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_OUTSRCQUENCHS);
		break;

	case PMID_NET_EICMP_INREDIRECTS:
		sneic->InRedirects = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_INREDIRECTS);
		break;

	case PMID_NET_EICMP_OUTREDIRECTS:
		sneic->OutRedirects = pcp_read_u64(values, 0,
						net_eicmp_metric_descs,
						NET_EICMP_OUTREDIRECTS);
		break;
	}
}

/*
 ***************************************************************************
 * Write TCP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_tcp_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_tcp
		*sntc = (struct stats_net_tcp *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sntc->ActiveOpens);
	pmiPutValue("network.tcp.activeopens", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sntc->PassiveOpens);
	pmiPutValue("network.tcp.passiveopens", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sntc->InSegs);
	pmiPutValue("network.tcp.insegs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sntc->OutSegs);
	pmiPutValue("network.tcp.outsegs", NULL, buf);
}

/*
 ***************************************************************************
 * Read TCP network statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_tcp_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_tcp
		*sntc = (struct stats_net_tcp *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_TCP_ACTIVEOPENS:
		sntc->ActiveOpens = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_TCP_ACTIVEOPENS);
		break;

	case PMID_NET_TCP_PASSIVEOPENS:
		sntc->PassiveOpens = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_TCP_PASSIVEOPENS);
		break;

	case PMID_NET_TCP_INSEGS:
		sntc->InSegs = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_TCP_INSEGS);
		break;

	case PMID_NET_TCP_OUTSEGS:
		sntc->OutSegs = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_TCP_OUTSEGS);
		break;
	}
}

/*
 ***************************************************************************
 * Write TCP network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_etcp_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_etcp
		*snetc = (struct stats_net_etcp *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snetc->AttemptFails);
	pmiPutValue("network.tcp.attemptfails", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snetc->EstabResets);
	pmiPutValue("network.tcp.estabresets", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snetc->RetransSegs);
	pmiPutValue("network.tcp.retranssegs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snetc->InErrs);
	pmiPutValue("network.tcp.inerrs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snetc->OutRsts);
	pmiPutValue("network.tcp.outrsts", NULL, buf);
}

/*
 ***************************************************************************
 * Read TCP network errors statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_etcp_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_etcp
		*snetc = (struct stats_net_etcp *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_ETCP_ATTEMPTFAILS:
		snetc->AttemptFails = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_ETCP_ATTEMPTFAILS);
		break;

	case PMID_NET_ETCP_ESTABRESETS:
		snetc->EstabResets = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_ETCP_ESTABRESETS);
		break;

	case PMID_NET_ETCP_RETRANSSEGS:
		snetc->RetransSegs = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_ETCP_RETRANSSEGS);
		break;

	case PMID_NET_ETCP_INERRS:
		snetc->InErrs = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_ETCP_INERRS);
		break;

	case PMID_NET_ETCP_OUTRSTS:
		snetc->OutRsts = pcp_read_u64(values, 0,
						net_etcp_metric_descs,
						NET_ETCP_OUTRSTS);
		break;
	}
}

/*
 ***************************************************************************
 * Write UDP network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_udp_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_udp
		*snuc = (struct stats_net_udp *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->InDatagrams);
	pmiPutValue("network.udp.indatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->OutDatagrams);
	pmiPutValue("network.udp.outdatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->NoPorts);
	pmiPutValue("network.udp.noports", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->InErrors);
	pmiPutValue("network.udp.inerrors", NULL, buf);
}

/*
 ***************************************************************************
 * Read UDP network statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_udp_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_udp
		*snuc = (struct stats_net_udp *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_UDP_INDATAGRAMS:
		snuc->InDatagrams = pcp_read_u64(values, 0,
						net_udp_metric_descs,
						NET_UDP_INDATAGRAMS);
		break;

	case PMID_NET_UDP_OUTDATAGRAMS:
		snuc->OutDatagrams = pcp_read_u64(values, 0,
						net_udp_metric_descs,
						NET_UDP_OUTDATAGRAMS);
		break;

	case PMID_NET_UDP_NOPORTS:
		snuc->NoPorts = pcp_read_u64(values, 0,
						net_udp_metric_descs,
						NET_UDP_NOPORTS);
		break;

	case PMID_NET_UDP_INERRORS:
		snuc->InErrors = pcp_read_u64(values, 0,
						net_udp_metric_descs,
						NET_UDP_INERRORS);
		break;
	}
}

/*
 ***************************************************************************
 * Write IPv6 network sockets statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_sock6_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_sock6
		*snsc = (struct stats_net_sock6 *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%u", snsc->tcp6_inuse);
	pmiPutValue("network.sockstat.tcp6.inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->udp6_inuse);
	pmiPutValue("network.sockstat.udp6.inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->raw6_inuse);
	pmiPutValue("network.sockstat.raw6.inuse", NULL, buf);

	snprintf(buf, sizeof(buf), "%u", snsc->frag6_inuse);
	pmiPutValue("network.sockstat.frag6.inuse", NULL, buf);
}

/*
 ***************************************************************************
 * Read IPv6 network sockets statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_sock6_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_sock6
		*snsc = (struct stats_net_sock6 *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_SOCK6_TCPINUSE:
		snsc->tcp6_inuse = pcp_read_u64(values, 0,
						net_sock6_metric_descs,
						NET_SOCK6_TCPINUSE);
		break;

	case PMID_NET_SOCK6_UDPINUSE:
		snsc->udp6_inuse = pcp_read_u64(values, 0,
						net_sock6_metric_descs,
						NET_SOCK6_UDPINUSE);
		break;

	case PMID_NET_SOCK6_RAWINUSE:
		snsc->raw6_inuse = pcp_read_u64(values, 0,
						net_sock6_metric_descs,
						NET_SOCK6_RAWINUSE);
		break;

	case PMID_NET_SOCK6_FRAGINUSE:
		snsc->frag6_inuse = pcp_read_u64(values, 0,
						net_sock6_metric_descs,
						NET_SOCK6_FRAGINUSE);
		break;
	}
}

/*
 ***************************************************************************
 * Write IPv6 network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_ip6_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_ip6
		*snic = (struct stats_net_ip6 *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", snic->InReceives6);
	pmiPutValue("network.ip6.inreceives", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->OutForwDatagrams6);
	pmiPutValue("network.ip6.outforwdatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->InDelivers6);
	pmiPutValue("network.ip6.indelivers", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->OutRequests6);
	pmiPutValue("network.ip6.outrequests", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->ReasmReqds6);
	pmiPutValue("network.ip6.reasmreqds", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->ReasmOKs6);
	pmiPutValue("network.ip6.reasmoks", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->InMcastPkts6);
	pmiPutValue("network.ip6.inmcastpkts", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->OutMcastPkts6);
	pmiPutValue("network.ip6.outmcastpkts", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->FragOKs6);
	pmiPutValue("network.ip6.fragoks", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", snic->FragCreates6);
	pmiPutValue("network.ip6.fragcreates", NULL, buf);
}

/*
 ***************************************************************************
 * Read IPv6 network statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_ip6_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_ip6
		*snic = (struct stats_net_ip6 *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_IP6_INRECEIVES:
		snic->InReceives6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_INRECEIVES);
		break;

	case PMID_NET_IP6_OUTFORWDATAGRAMS:
		snic->OutForwDatagrams6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_OUTFORWDATAGRAMS);
		break;

	case PMID_NET_IP6_INDELIVERS:
		snic->InDelivers6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_INDELIVERS);
		break;

	case PMID_NET_IP6_OUTREQUESTS:
		snic->OutRequests6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_OUTREQUESTS);
		break;

	case PMID_NET_IP6_REASMREQDS:
		snic->ReasmReqds6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_REASMREQDS);
		break;

	case PMID_NET_IP6_REASMOKS:
		snic->ReasmOKs6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_REASMOKS);
		break;

	case PMID_NET_IP6_INMCASTPKTS:
		snic->InMcastPkts6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_INMCASTPKTS);
		break;

	case PMID_NET_IP6_OUTMCASTPKTS:
		snic->OutMcastPkts6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_OUTMCASTPKTS);
		break;

	case PMID_NET_IP6_FRAGOKS:
		snic->FragOKs6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_FRAGOKS);
		break;

	case PMID_NET_IP6_FRAGCREATES:
		snic->FragCreates6 = pcp_read_u64(values, 0,
						net_ip6_metric_descs,
						NET_IP6_FRAGCREATES);
		break;
	}
}

/*
 ***************************************************************************
 * Write IPv6 network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eip6_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_eip6
		*sneic = (struct stats_net_eip6 *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", sneic->InHdrErrors6);
	pmiPutValue("network.ip6.inhdrerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InAddrErrors6);
	pmiPutValue("network.ip6.inaddrerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InUnknownProtos6);
	pmiPutValue("network.ip6.inunknownprotos", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InTooBigErrors6);
	pmiPutValue("network.ip6.intoobigerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InDiscards6);
	pmiPutValue("network.ip6.indiscards", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->OutDiscards6);
	pmiPutValue("network.ip6.outdiscards", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InNoRoutes6);
	pmiPutValue("network.ip6.innoroutes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->OutNoRoutes6);
	pmiPutValue("network.ip6.outnoroutes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->ReasmFails6);
	pmiPutValue("network.ip6.reasmfails", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->FragFails6);
	pmiPutValue("network.ip6.fragfails", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", sneic->InTruncatedPkts6);
	pmiPutValue("network.ip6.intruncatedpkts", NULL, buf);
}

/*
 ***************************************************************************
 * Read IPv6 network errors statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_eip6_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_eip6
		*sneic = (struct stats_net_eip6 *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_EIP6_INHDRERRORS:
		sneic->InHdrErrors6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_INHDRERRORS);
		break;

	case PMID_NET_EIP6_INADDRERRORS:
		sneic->InAddrErrors6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_INADDRERRORS);
		break;

	case PMID_NET_EIP6_INUNKNOWNPROTOS:
		sneic->InUnknownProtos6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_INUNKNOWNPROTOS);
		break;

	case PMID_NET_EIP6_INTOOBIGERRORS:
		sneic->InTooBigErrors6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_INTOOBIGERRORS);
		break;

	case PMID_NET_EIP6_INDISCARDS:
		sneic->InDiscards6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_INDISCARDS);
		break;

	case PMID_NET_EIP6_OUTDISCARDS:
		sneic->OutDiscards6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_OUTDISCARDS);
		break;

	case PMID_NET_EIP6_INNOROUTES:
		sneic->InNoRoutes6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_INNOROUTES);
		break;

	case PMID_NET_EIP6_OUTNOROUTES:
		sneic->OutNoRoutes6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_OUTNOROUTES);
		break;

	case PMID_NET_EIP6_REASMFAILS:
		sneic->ReasmFails6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_REASMFAILS);
		break;

	case PMID_NET_EIP6_FRAGFAILS:
		sneic->FragFails6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_FRAGFAILS);
		break;

	case PMID_NET_EIP6_INTRUNCATEDPKTS:
		sneic->InTruncatedPkts6 = pcp_read_u64(values, 0,
						net_eip6_metric_descs,
						NET_EIP6_INTRUNCATEDPKTS);
		break;
	}
}

/*
 ***************************************************************************
 * Write ICMPv6 network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_icmp6_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_icmp6
		*snic = (struct stats_net_icmp6 *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InMsgs6);
	pmiPutValue("network.icmp6.inmsgs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutMsgs6);
	pmiPutValue("network.icmp6.outmsgs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InEchos6);
	pmiPutValue("network.icmp6.inechos", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InEchoReplies6);
	pmiPutValue("network.icmp6.inechoreplies", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutEchoReplies6);
	pmiPutValue("network.icmp6.outechoreplies", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InGroupMembQueries6);
	pmiPutValue("network.icmp6.ingroupmembqueries", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InGroupMembResponses6);
	pmiPutValue("network.icmp6.ingroupmembresponses", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutGroupMembResponses6);
	pmiPutValue("network.icmp6.outgroupmembresponses", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InGroupMembReductions6);
	pmiPutValue("network.icmp6.ingroupmembreductions", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutGroupMembReductions6);
	pmiPutValue("network.icmp6.outgroupmembreductions", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InRouterSolicits6);
	pmiPutValue("network.icmp6.inroutersolicits", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutRouterSolicits6);
	pmiPutValue("network.icmp6.outroutersolicits", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InRouterAdvertisements6);
	pmiPutValue("network.icmp6.inrouteradvertisements", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InNeighborSolicits6);
	pmiPutValue("network.icmp6.inneighborsolicits", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutNeighborSolicits6);
	pmiPutValue("network.icmp6.outneighborsolicits", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->InNeighborAdvertisements6);
	pmiPutValue("network.icmp6.inneighboradvertisements", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snic->OutNeighborAdvertisements6);
	pmiPutValue("network.icmp6.outneighboradvertisements", NULL, buf);
}

/*
 ***************************************************************************
 * Read ICMPv6 network statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_icmp6_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_icmp6
		*snic = (struct stats_net_icmp6 *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_ICMP6_INMSGS:
		snic->InMsgs6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INMSGS);
		break;

	case PMID_NET_ICMP6_OUTMSGS:
		snic->OutMsgs6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_OUTMSGS);
		break;

	case PMID_NET_ICMP6_INECHOS:
		snic->InEchos6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INECHOS);
		break;

	case PMID_NET_ICMP6_INECHOREPLIES:
		snic->InEchoReplies6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INECHOREPLIES);
		break;

	case PMID_NET_ICMP6_OUTECHOREPLIES:
		snic->OutEchoReplies6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_OUTECHOREPLIES);
		break;

	case PMID_NET_ICMP6_INGROUPMEMBQUERIES:
		snic->InGroupMembQueries6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INGROUPMEMBQUERIES);
		break;

	case PMID_NET_ICMP6_INGROUPMEMBRESPONSES:
		snic->InGroupMembResponses6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INGROUPMEMBRESPONSES);
		break;

	case PMID_NET_ICMP6_OUTGROUPMEMBRESPONSES:
		snic->OutGroupMembResponses6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_OUTGROUPMEMBRESPONSES);
		break;

	case PMID_NET_ICMP6_INGROUPMEMBREDUCTIONS:
		snic->InGroupMembReductions6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INGROUPMEMBREDUCTIONS);
		break;

	case PMID_NET_ICMP6_OUTGROUPMEMBREDUCTIONS:
		snic->OutGroupMembReductions6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_OUTGROUPMEMBREDUCTIONS);
		break;

	case PMID_NET_ICMP6_INROUTERSOLICITS:
		snic->InRouterSolicits6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INROUTERSOLICITS);
		break;

	case PMID_NET_ICMP6_OUTROUTERSOLICITS:
		snic->OutRouterSolicits6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_OUTROUTERSOLICITS);
		break;

	case PMID_NET_ICMP6_INROUTERADVERTISEMENTS:
		snic->InRouterAdvertisements6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INROUTERADVERTISEMENTS);
		break;

	case PMID_NET_ICMP6_INNEIGHBORSOLICITS:
		snic->InNeighborSolicits6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INNEIGHBORSOLICITS);
		break;

	case PMID_NET_ICMP6_OUTNEIGHBORSOLICITS:
		snic->OutNeighborSolicits6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_OUTNEIGHBORSOLICITS);
		break;

	case PMID_NET_ICMP6_INNEIGHBORADVERTISEMENTS:
		snic->InNeighborAdvertisements6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_INNEIGHBORADVERTISEMENTS);
		break;

	case PMID_NET_ICMP6_OUTNEIGHBORADVERTISEMENTS:
		snic->OutNeighborAdvertisements6 = pcp_read_u64(values, 0,
						net_icmp6_metric_descs,
						NET_ICMP6_OUTNEIGHBORADVERTISEMENTS);
		break;
	}
}

/*
 ***************************************************************************
 * Write ICMPv6 network errors statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_eicmp6_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_eicmp6
		*sneic = (struct stats_net_eicmp6 *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InErrors6);
	pmiPutValue("network.icmp6.inerrors", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InDestUnreachs6);
	pmiPutValue("network.icmp6.indestunreachs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutDestUnreachs6);
	pmiPutValue("network.icmp6.outdestunreachs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InTimeExcds6);
	pmiPutValue("network.icmp6.intimeexcds", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutTimeExcds6);
	pmiPutValue("network.icmp6.outtimeexcds", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InParmProblems6);
	pmiPutValue("network.icmp6.inparmproblems", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutParmProblems6);
	pmiPutValue("network.icmp6.outparmproblems", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InRedirects6);
	pmiPutValue("network.icmp6.inredirects", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutRedirects6);
	pmiPutValue("network.icmp6.outredirects", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->InPktTooBigs6);
	pmiPutValue("network.icmp6.inpkttoobigs", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sneic->OutPktTooBigs6);
	pmiPutValue("network.icmp6.outpkttoobigs", NULL, buf);
}

/*
 ***************************************************************************
 * Read ICMP6 network errors statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_eicmp6_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_eicmp6
		*sneic = (struct stats_net_eicmp6 *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_EICMP6_INERRORS:
		sneic->InErrors6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_INERRORS);
		break;

	case PMID_NET_EICMP6_INDESTUNREACHS:
		sneic->InDestUnreachs6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_INDESTUNREACHS);
		break;

	case PMID_NET_EICMP6_OUTDESTUNREACHS:
		sneic->OutDestUnreachs6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_OUTDESTUNREACHS);
		break;

	case PMID_NET_EICMP6_INTIMEEXCDS:
		sneic->InTimeExcds6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_INTIMEEXCDS);
		break;

	case PMID_NET_EICMP6_OUTTIMEEXCDS:
		sneic->OutTimeExcds6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_OUTTIMEEXCDS);
		break;

	case PMID_NET_EICMP6_INPARMPROBLEMS:
		sneic->InParmProblems6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_INPARMPROBLEMS);
		break;

	case PMID_NET_EICMP6_OUTPARMPROBLEMS:
		sneic->OutParmProblems6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_OUTPARMPROBLEMS);
		break;

	case PMID_NET_EICMP6_INREDIRECTS:
		sneic->InRedirects6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_INREDIRECTS);
		break;

	case PMID_NET_EICMP6_OUTREDIRECTS:
		sneic->OutRedirects6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_OUTREDIRECTS);
		break;

	case PMID_NET_EICMP6_INPKTTOOBIGS:
		sneic->InPktTooBigs6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_INPKTTOOBIGS);
		break;

	case PMID_NET_EICMP6_OUTPKTTOOBIGS:
		sneic->OutPktTooBigs6 = pcp_read_u64(values, 0,
						net_eicmp6_metric_descs,
						NET_EICMP6_OUTPKTTOOBIGS);
		break;
	}
}

/*
 ***************************************************************************
 * Write UDPv6 network statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_net_udp6_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_net_udp6
		*snuc = (struct stats_net_udp6 *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->InDatagrams6);
	pmiPutValue("network.udp6.indatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->OutDatagrams6);
	pmiPutValue("network.udp6.outdatagrams", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->NoPorts6);
	pmiPutValue("network.udp6.noports", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", (unsigned long long) snuc->InErrors6);
	pmiPutValue("network.udp6.inerrors", NULL, buf);
}

/*
 ***************************************************************************
 * Read UDPv6 network statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_net_udp6_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_net_udp6
		*snuc = (struct stats_net_udp6 *) a->buf[curr];

	switch (values->pmid) {

	case PMID_NET_UDP6_INDATAGRAMS:
		snuc->InDatagrams6 = pcp_read_u64(values, 0,
						net_udp6_metric_descs,
						NET_UDP6_INDATAGRAMS);
		break;

	case PMID_NET_UDP6_OUTDATAGRAMS:
		snuc->OutDatagrams6 = pcp_read_u64(values, 0,
						net_udp6_metric_descs,
						NET_UDP6_OUTDATAGRAMS);
		break;

	case PMID_NET_UDP6_NOPORTS:
		snuc->NoPorts6 = pcp_read_u64(values, 0,
						net_udp6_metric_descs,
						NET_UDP6_NOPORTS);
		break;

	case PMID_NET_UDP6_INERRORS:
		snuc->InErrors6 = pcp_read_u64(values, 0,
						net_udp6_metric_descs,
						NET_UDP6_INERRORS);
		break;
	}
}

/*
 ***************************************************************************
 * Write CPU frequency statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_cpufreq_stats(struct activity *a, int curr)
{
	int i;
	struct stats_pwr_cpufreq *spc;
	char buf[64], cpuno[64];

	for (i = 0; (i < a->nr[curr]) && (i < a->bitmap->b_size + 1); i++) {

		spc = (struct stats_pwr_cpufreq *) ((char *) a->buf[curr] + i * a->msize);

		/* Should current CPU (including CPU "all") be displayed? */
		if (!(a->bitmap->b_array[i >> 3] & (1 << (i & 0x07))))
			/* No */
			continue;

		if (!i) {
			/* This is CPU "all" */
			continue;
		}
		else {
			sprintf(cpuno, "cpu%d", i - 1);
		}

		snprintf(buf, sizeof(buf), "%f", ((double) spc->cpufreq) / 100);
		pmiPutValue("hinv.cpu.clock", cpuno, buf);
	}
}

/*
 ***************************************************************************
 * Read CPU frequency statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_pwr_cpufreq_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0 // TODO: per-CPU
	switch (values->pmid) {
		case PMID_POWER_PERCPU_CLOCK:
			break;
	}
#endif
}

/*
 ***************************************************************************
 * Write fan statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_fan_stats(struct activity *a, int curr)
{
	int i;
	struct stats_pwr_fan *spc;
	char buf[64], instance[32];

	for (i = 0; i < a->nr[curr]; i++) {

		spc = (struct stats_pwr_fan *) ((char *) a->buf[curr] + i * a->msize);
		sprintf(instance, "fan%d", i + 1);

		snprintf(buf, sizeof(buf), "%llu",
			 (unsigned long long) spc->rpm);
		pmiPutValue("power.fan.rpm", instance, buf);

		snprintf(buf, sizeof(buf), "%llu",
			 (unsigned long long) (spc->rpm - spc->rpm_min));
		pmiPutValue("power.fan.drpm", instance, buf);

		snprintf(buf, sizeof(buf), "%s", spc->device);
		pmiPutValue("power.fan.device", instance, buf);
	}
}

/*
 ***************************************************************************
 * Read fan statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_power_fan_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0 // TODO: per-FAN
	struct stats_pwr_fan *spc;

	switch (values->pmid) {
		case PMID_POWER_FAN_RPM:
		case PMID_POWER_FAN_DRPM:
		case PMID_POWER_FAN_DEVICE:
			break;
	}
#endif
}

/*
 ***************************************************************************
 * Write temperature statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_temp_stats(struct activity *a, int curr)
{
	int i;
	struct stats_pwr_temp *spc;
	char buf[64], instance[32];

	for (i = 0; i < a->nr[curr]; i++) {

		spc = (struct stats_pwr_temp *) ((char *) a->buf[curr] + i * a->msize);
		sprintf(instance, "temp%d", i + 1);

		snprintf(buf, sizeof(buf), "%f", spc->temp);
		pmiPutValue("power.temp.celsius", instance, buf);

		snprintf(buf, sizeof(buf), "%f",
			 (spc->temp_max - spc->temp_min) ?
			 (spc->temp - spc->temp_min) / (spc->temp_max - spc->temp_min) * 100 :
			 0.0);
		pmiPutValue("power.temp.percent", instance, buf);

		snprintf(buf, sizeof(buf), "%s",
			spc->device);
		pmiPutValue("power.temp.device", instance, buf);
	}
}

/*
 ***************************************************************************
 * Read temperature statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_power_temp_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0 // TODO: per-TEMP
	struct stats_pwr_temp *spc;

	switch (values->pmid) {
		case PMID_POWER_TEMP_CELSIUS:
		case PMID_POWER_TEMP_PERCENT:
		case PMID_POWER_TEMP_DEVICE:
			break;
	}
#endif
}

/*
 ***************************************************************************
 * Write voltage inputs statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_in_stats(struct activity *a, int curr)
{
	int i;
	struct stats_pwr_in *spc;
	char buf[64], instance[32];

	for (i = 0; i < a->nr[curr]; i++) {

		spc = (struct stats_pwr_in *) ((char *) a->buf[curr] + i * a->msize);
		sprintf(instance, "in%d", i);

		snprintf(buf, sizeof(buf), "%f",
			 spc->in);
		pmiPutValue("power.in.voltage", instance, buf);

		snprintf(buf, sizeof(buf), "%f",
			 (spc->in_max - spc->in_min) ?
			 (spc->in - spc->in_min) / (spc->in_max - spc->in_min) * 100 :
			 0.0);
		pmiPutValue("power.in.percent", instance, buf);

		snprintf(buf, sizeof(buf), "%s",
			spc->device);
		pmiPutValue("power.in.device", instance, buf);
	}
}

/*
 ***************************************************************************
 * Read voltage input statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_power_in_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0 // TODO: per-vin
	struct stats_pwr_in *spc;

	switch (values->pmid) {
		case PMID_POWER_IN_VOLTAGE:
		case PMID_POWER_IN_PERCENT:
		case PMID_POWER_IN_DEVICE:
			break;
	}
#endif
}

/*
 * **************************************************************************
 * Write batteries statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_bat_stats(struct activity *a, int curr)
{
	int i;
	struct stats_pwr_bat *spbc;
	char buf[64], bat_name[16];

	for (i = 0; i < a->nr[curr]; i++) {

		spbc = (struct stats_pwr_bat *) ((char *) a->buf[curr] + i * a->msize);

		snprintf(bat_name, sizeof(bat_name), "BAT%d", (int) spbc->bat_id);

		snprintf(buf, sizeof(buf), "%u", (unsigned int) spbc->capacity);
		pmiPutValue("power.bat.capacity", bat_name, buf);

		/* Battery status code should not be greater than or equal to BAT_STS_NR */
		if (spbc->status >= BAT_STS_NR) {
			spbc->status = 0;
		}

		//		TODO
//		snprintf(buf, sizeof(buf), "%s", bat_status[(unsigned int) spbc->status]);
		pmiPutValue("power.bat.status", bat_name, buf);
	}
}

/*
 ***************************************************************************
 * Read batteries statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_power_bat_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0 // TODO: per-battery
	struct stats_pwr_bat *spbc;

	switch (values->pmid) {
		case PMID_POWER_BAT_CAPACITY:
		case PMID_POWER_BAT_STATUS:
			break;
	}
#endif
}

/*
 ***************************************************************************
 * Write huge pages statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_huge_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_huge
		*smc = (struct stats_huge *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%llu", smc->frhkb * 1024);
	pmiPutValue("mem.util.hugepagesFreeBytes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", smc->tlhkb * 1024);
	pmiPutValue("mem.util.hugepagesTotalBytes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", smc->rsvdhkb * 1024);
	pmiPutValue("mem.util.hugepagesRsvdBytes", NULL, buf);

	snprintf(buf, sizeof(buf), "%llu", smc->surphkb * 1024);
	pmiPutValue("mem.util.hugepagesSurpBytes", NULL, buf);
}

/*
 ***************************************************************************
 * Read huge pages statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_huge_stats(pmValueSet *values, struct activity *a, int curr)
{
	struct stats_huge
		*smc = (struct stats_huge *) a->buf[curr];

	switch (values->pmid) {

	case PMID_MEM_HUGE_TOTALBYTES:
		smc->tlhkb = pcp_read_u64(values, 0, mem_huge_metric_descs,
						MEM_HUGE_TOTALBYTES);
		smc->tlhkb /= 1024;
		break;

	case PMID_MEM_HUGE_FREEBYTES:
		smc->frhkb = pcp_read_u64(values, 0, mem_huge_metric_descs,
						MEM_HUGE_FREEBYTES);
		smc->frhkb /= 1024;
		break;

	case PMID_MEM_HUGE_RSVDBYTES:
		smc->rsvdhkb = pcp_read_u64(values, 0, mem_huge_metric_descs,
						MEM_HUGE_RSVDBYTES);
		smc->rsvdhkb /= 1024;
		break;

	case PMID_MEM_HUGE_SURPBYTES:
		smc->surphkb = pcp_read_u64(values, 0, mem_huge_metric_descs,
						MEM_HUGE_SURPBYTES);
		smc->surphkb /= 1024;
		break;
	}
}

/*
 ***************************************************************************
 * Write USB devices in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_pwr_usb_stats(struct activity *a, int curr)
{
	int i;
	struct stats_pwr_usb *suc;
	char buf[64], instance[32];

	for (i = 0; i < a->nr[curr]; i++) {

		suc = (struct stats_pwr_usb *) ((char *) a->buf[curr] + i * a->msize);
		sprintf(instance, "usb%d", i);

		snprintf(buf, sizeof(buf), "%u", suc->bus_nr);
		pmiPutValue("power.usb.bus", instance, buf);

		snprintf(buf, sizeof(buf), "%x", suc->vendor_id);
		pmiPutValue("power.usb.vendorId", instance, buf);

		snprintf(buf, sizeof(buf), "%x", suc->product_id);
		pmiPutValue("power.usb.productId", instance, buf);

		snprintf(buf, sizeof(buf), "%u", suc->bmaxpower << 1);
		pmiPutValue("power.usb.maxpower", instance, buf);

		snprintf(buf, sizeof(buf), "%s", suc->manufacturer);
		pmiPutValue("power.usb.manufacturer", instance, buf);

		snprintf(buf, sizeof(buf), "%s", suc->product);
		pmiPutValue("power.usb.productName", instance, buf);
	}
}

/*
 ***************************************************************************
 * Read USB devices from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_power_usb_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0 // TODO: per-USB
	switch (values->pmid) {
		case PMID_POWER_USB_BUS:
		case PMID_POWER_USB_VENDORID:
		case PMID_POWER_USB_PRODUCTID:
		case PMID_POWER_USB_MAXPOWER:
		case PMID_POWER_USB_MANUFACTURER:
		case PMID_POWER_USB_PRODUCTNAME:
			break;
	}
#endif
}

/*
 ***************************************************************************
 * Write filesystem statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_filesystem_stats(struct activity *a, int curr)
{
	int i;
	struct stats_filesystem *sfc;
	char buf[64];
	char *dev_name;

	for (i = 0; i < a->nr[curr]; i++) {
		sfc = (struct stats_filesystem *) ((char *) a->buf[curr] + i * a->msize);

		/* Get name to display (persistent or standard fs name, or mount point) */
		dev_name = get_fs_name_to_display(a, flags, sfc);

		if (a->item_list != NULL) {
			/* A list of devices has been entered on the command line */
			if (!search_list_item(a->item_list, dev_name))
				/* Device not found */
				continue;
		}

		snprintf(buf, sizeof(buf), "%llu", sfc->f_blocks / 1024);
		pmiPutValue("filesys.capacity", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", sfc->f_bfree / 1024);
		pmiPutValue("filesys.free", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu",
			 (sfc->f_blocks - sfc->f_bfree) / 1024);
		pmiPutValue("filesys.used", dev_name, buf);

		snprintf(buf, sizeof(buf), "%f",
			 sfc->f_blocks ? SP_VALUE(sfc->f_bfree, sfc->f_blocks, sfc->f_blocks)
				       : 0.0);
		pmiPutValue("filesys.full", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", sfc->f_files);
		pmiPutValue("filesys.maxfiles", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", sfc->f_ffree);
		pmiPutValue("filesys.freefiles", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", sfc->f_files - sfc->f_ffree);
		pmiPutValue("filesys.usedfiles", dev_name, buf);

		snprintf(buf, sizeof(buf), "%llu", sfc->f_bavail / 1024);
		pmiPutValue("filesys.avail", dev_name, buf);
	}
}

/*
 ***************************************************************************
 * Read filesystem statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_filesystem_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0 // TODO: per-filesystem
	struct stats_filesystem *sfc;

	switch (values->pmid) {
		case PMID_FILESYS_CAPACITY:
		case PMID_FILESYS_FREE:
		case PMID_FILESYS_USED:
		case PMID_FILESYS_FULL:
		case PMID_FILESYS_MAXFILES:
		case PMID_FILESYS_FREEFILES:
		case PMID_FILESYS_USEDFILES:
		case PMID_FILESYS_AVAIL:
			break;
	}
#endif
}

/*
 ***************************************************************************
 * Write Fibre Channel HBA statistics in PCP format.
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_fchost_stats(struct activity *a, int curr)
{
	int i;
	struct stats_fchost *sfcc;
	char buf[64];

	for (i = 0; i < a->nr[curr]; i++) {

		sfcc = (struct stats_fchost *) ((char *) a->buf[curr] + i * a->msize);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sfcc->f_rxframes);
		pmiPutValue("fchost.in.frames", sfcc->fchost_name, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sfcc->f_txframes);
		pmiPutValue("fchost.out.frames", sfcc->fchost_name, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sfcc->f_rxwords * 4);
		pmiPutValue("fchost.in.bytes", sfcc->fchost_name, buf);

		snprintf(buf, sizeof(buf), "%llu", (unsigned long long) sfcc->f_txwords * 4);
		pmiPutValue("fchost.out.bytes", sfcc->fchost_name, buf);
	}
}

/*
 ***************************************************************************
 * Read Fibre Channel HBA statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_fchost_stats(pmValueSet *values, struct activity *a, int curr)
{
#if 0 // TODO: per-fchost
	struct stats_fchost *sfcc;

	switch (values->pmid) {
		case PMID_FCHOST_INFRAMES:
		case PMID_FCHOST_OUTFRAMES:
		case PMID_FCHOST_INBYTES:
		case PMID_FCHOST_OUTBYTES:
			break;
	}
#endif
}

/*
 ***************************************************************************
 * Write pressure-stall CPU statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_psicpu_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_psi_cpu
		*psic = (struct stats_psi_cpu *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_acpu_10 / 100);
	pmiPutValue("kernel.all.pressure.cpu.some.avg", "10 second", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_acpu_60 / 100);
	pmiPutValue("kernel.all.pressure.cpu.some.avg", "1 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_acpu_300 / 100);
	pmiPutValue("kernel.all.pressure.cpu.some.avg", "5 minute", buf);

	snprintf(buf, sizeof(buf), "%llu", psic->some_cpu_total);
	pmiPutValue("kernel.all.pressure.cpu.some.total", NULL, buf);
}

/*
 ***************************************************************************
 * Read pressure-stall CPU statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_psicpu_stats(pmValueSet *values, struct activity *a, int curr)
{
	int i;
	struct stats_psi_cpu
		*psic = (struct stats_psi_cpu *) a->buf[curr];

	switch (values->pmid) {

	case PMID_PSI_CPU_SOMETOTAL:
		psic->some_cpu_total = pcp_read_u64(values, 0,
						psi_cpu_metric_descs,
						PSI_CPU_SOMETOTAL);
		break;

	case PMID_PSI_CPU_SOMEAVG:
		for (i = 0; i < values->numval; i++) {
		       	switch (values->vlist[i].inst) {

			case 10:	/* 10 second */
				psic->some_acpu_10 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_cpu_metric_descs,
						PSI_CPU_SOMEAVG);
				break;

			case 60:	/* 1 minute */
				psic->some_acpu_60 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_cpu_metric_descs,
						PSI_CPU_SOMEAVG);
				break;

			case 300:	/* 5 minute */
				psic->some_acpu_300 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_cpu_metric_descs,
						PSI_CPU_SOMEAVG);
				break;
			}
		}
		break;
	}
}

/*
 ***************************************************************************
 * Write pressure-stall I/O statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_psiio_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_psi_io
		*psic = (struct stats_psi_io *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_aio_10 / 100);
	pmiPutValue("kernel.all.pressure.io.some.avg", "10 second", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_aio_60 / 100);
	pmiPutValue("kernel.all.pressure.io.some.avg", "1 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_aio_300 / 100);
	pmiPutValue("kernel.all.pressure.io.some.avg", "5 minute", buf);

	snprintf(buf, sizeof(buf), "%llu", psic->some_io_total);
	pmiPutValue("kernel.all.pressure.io.some.total", NULL, buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_aio_10 / 100);
	pmiPutValue("kernel.all.pressure.io.full.avg", "10 second", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_aio_60 / 100);
	pmiPutValue("kernel.all.pressure.io.full.avg", "1 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_aio_300 / 100);
	pmiPutValue("kernel.all.pressure.io.full.avg", "5 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_io_total);
	pmiPutValue("kernel.all.pressure.io.full.total", NULL, buf);
}

/*
 ***************************************************************************
 * Read pressure-stall I/O statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_psiio_stats(pmValueSet *values, struct activity *a, int curr)
{
	int i;
	struct stats_psi_io
		*psiio = (struct stats_psi_io *) a->buf[curr];

	switch (values->pmid) {

	case PMID_PSI_IO_SOMETOTAL:
		psiio->some_io_total = pcp_read_u64(values, 0,
						psi_io_metric_descs,
						PSI_IO_SOMETOTAL);
		break;

	case PMID_PSI_IO_SOMEAVG:
		for (i = 0; i < values->numval; i++) {

		       	switch (values->vlist[i].inst) {
			case 10:	/* 10 second */
				psiio->some_aio_10 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_io_metric_descs,
						PSI_IO_SOMEAVG);
				break;

			case 60:	/* 1 minute */
				psiio->some_aio_60 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_io_metric_descs,
						PSI_IO_SOMEAVG);
				break;

			case 300:	/* 5 minute */
				psiio->some_aio_300 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_io_metric_descs,
						PSI_IO_SOMEAVG);
				break;
			}
		}
		break;

	case PMID_PSI_IO_FULLTOTAL:
		psiio->full_io_total = pcp_read_u64(values, 0,
						psi_io_metric_descs,
						PSI_IO_FULLTOTAL);
		break;

	case PMID_PSI_IO_FULLAVG:
		for (i = 0; i < values->numval; i++) {
		       	switch (values->vlist[i].inst) {

			case 10:	/* 10 second */
				psiio->full_aio_10 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_io_metric_descs,
						PSI_IO_FULLAVG);
				break;

			case 60:	/* 1 minute */
				psiio->full_aio_60 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_io_metric_descs,
						PSI_IO_FULLAVG);
				break;

			case 300:	/* 5 minute */
				psiio->full_aio_300 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_io_metric_descs,
						PSI_IO_FULLAVG);
				break;
			}
		}
		break;
	}
}

/*
 ***************************************************************************
 * Write pressure-stall memory statistics in PCP format
 *
 * IN:
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
__print_funct_t pcp_print_psimem_stats(struct activity *a, int curr)
{
	char buf[64];
	struct stats_psi_mem
		*psic = (struct stats_psi_mem *) a->buf[curr];

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_amem_10 / 100);
	pmiPutValue("kernel.all.pressure.memory.some.avg", "10 second", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_amem_60 / 100);
	pmiPutValue("kernel.all.pressure.memory.some.avg", "1 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->some_amem_300 / 100);
	pmiPutValue("kernel.all.pressure.memory.some.avg", "5 minute", buf);

	snprintf(buf, sizeof(buf), "%llu", psic->some_mem_total);
	pmiPutValue("kernel.all.pressure.memory.some.total", NULL, buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_amem_10 / 100);
	pmiPutValue("kernel.all.pressure.memory.full.avg", "10 second", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_amem_60 / 100);
	pmiPutValue("kernel.all.pressure.memory.full.avg", "1 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_amem_300 / 100);
	pmiPutValue("kernel.all.pressure.memory.full.avg", "5 minute", buf);

	snprintf(buf, sizeof(buf), "%f", (double) psic->full_mem_total);
	pmiPutValue("kernel.all.pressure.memory.full.total", NULL, buf);
}

/*
 ***************************************************************************
 * Read pressure-stall memory statistics from PCP format.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @a		Activity structure with statistics.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_psimem_stats(pmValueSet *values, struct activity *a, int curr)
{
	int i;
	struct stats_psi_mem
		*psim = (struct stats_psi_mem *) a->buf[curr];

	switch (values->pmid) {

	case PMID_PSI_MEM_SOMETOTAL:
		psim->some_mem_total = pcp_read_u64(values, 0,
						psi_mem_metric_descs,
						PSI_MEM_SOMETOTAL);
		break;

	case PMID_PSI_MEM_SOMEAVG:
		for (i = 0; i < values->numval; i++) {

		       	switch (values->vlist[i].inst) {
			case 10:	/* 10 second */
				psim->some_amem_10 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_mem_metric_descs,
						PSI_MEM_SOMEAVG);
				break;

			case 60:	/* 1 minute */
				psim->some_amem_60 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_mem_metric_descs,
						PSI_MEM_SOMEAVG);
				break;

			case 300:	/* 5 minute */
				psim->some_amem_300 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_mem_metric_descs,
						PSI_MEM_SOMEAVG);
				break;
			}
		}
		break;

	case PMID_PSI_MEM_FULLTOTAL:
		psim->full_mem_total = pcp_read_u64(values, 0,
						psi_mem_metric_descs,
						PSI_MEM_FULLTOTAL);
		break;

	case PMID_PSI_MEM_FULLAVG:
		for (i = 0; i < values->numval; i++) {
		       	switch (values->vlist[i].inst) {

			case 10:	/* 10 second */
				psim->full_amem_10 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_mem_metric_descs,
						PSI_MEM_FULLAVG);
				break;

			case 60:	/* 1 minute */
				psim->full_amem_60 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_mem_metric_descs,
						PSI_MEM_FULLAVG);
				break;

			case 300:	/* 5 minute */
				psim->full_amem_300 = 100 * (unsigned long)
					pcp_read_float(values, i,
						psi_mem_metric_descs,
						PSI_MEM_FULLAVG);
				break;
			}
		}
		break;
	}
}

/*
 ***************************************************************************
 * Read PCP metric valuesets into corresponding file header fields.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @file_hdr	File header structure housing global values.
 ***************************************************************************
 */
void pcp_read_file_header_stats(pmValueSet *values, struct file_header *file_hdr)
{
	char		*s;

	/*
	 * Metrics that augment the information from the PCP archive label.
	 * The label provides sa_ust_time (start time) and sa_tzname ($TZ).
	 */

	switch (values->pmid) {

		case PMID_FILE_HEADER_CPU_COUNT:
			file_hdr->sa_cpu_nr = pcp_read_u32(values, 0,
						file_header_metrics.descs,
						FILE_HEADER_CPU_COUNT);
			break;

		case PMID_FILE_HEADER_KERNEL_HERTZ:
			file_hdr->sa_hz = pcp_read_u32(values, 0,
						file_header_metrics.descs,
						FILE_HEADER_KERNEL_HERTZ);
			break;

		case PMID_FILE_HEADER_UNAME_SYSNAME:
			s = pcp_read_str(values, 0, file_header_metrics.descs,
						FILE_HEADER_UNAME_SYSNAME);
			pmsprintf(file_hdr->sa_sysname, UTSNAME_LEN, "%s", s);
			free(s);
			break;

		case PMID_FILE_HEADER_UNAME_RELEASE:
			s = pcp_read_str(values, 0, file_header_metrics.descs,
						FILE_HEADER_UNAME_RELEASE);
			pmsprintf(file_hdr->sa_release, UTSNAME_LEN, "%s", s);
			free(s);
			break;

		case PMID_FILE_HEADER_UNAME_NODENAME:
			s = pcp_read_str(values, 0, file_header_metrics.descs,
						FILE_HEADER_UNAME_NODENAME);
			pmsprintf(file_hdr->sa_nodename, UTSNAME_LEN, "%s", s);
			free(s);
			break;

		case PMID_FILE_HEADER_UNAME_MACHINE:
			s = pcp_read_str(values, 0, file_header_metrics.descs,
						FILE_HEADER_UNAME_MACHINE);
			pmsprintf(file_hdr->sa_machine, UTSNAME_LEN, "%s", s);
			free(s);
			break;
	}
}

/*
 ***************************************************************************
 * Read PCP metric valuesets into corresponding record header fields.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_record_header_stats(pmValueSet *values, int curr)
{
	struct record_header *header = &record_hdr[curr];

	/*
	 * Metrics that augment the information from the PCP archive label.
	 * The label provides sa_ust_time (start time) and sa_tzname ($TZ).
	 */

	switch (values->pmid) {

		case PMID_RECORD_HEADER_KERNEL_UPTIME:
			header->uptime_cs = (unsigned long long)
					(100.0 * pcp_read_double(values, 0,
						record_header_metrics.descs,
						RECORD_HEADER_KERNEL_UPTIME));
			break;
	}
}

/*
 ***************************************************************************
 * Insert PCP metric valuesets into corresponding activity buffers,
 * and/or file header structure.
 *
 * As efficiently as possible; uses a single switch statement across
 * the globally unique pmID field - an unsigned integer - allows the
 * compiler to optimise lookup for us without ancillary structures.
 * The use of a single case statement has the additional benefit of
 * ensuring we never (accidentally) get duplicate metric identifiers
 * in the internal tables.
 *
 * IN:
 * @values	Metric values set with statistic values.
 * @header	File header structure housing global values.
 * @curr	Index in array for current sample statistics.
 ***************************************************************************
 */
void pcp_read_stats(pmValueSet *values, struct file_header *header, int curr)
{
	switch (values->pmid) {

		case PMID_FILE_HEADER_CPU_COUNT:
		case PMID_FILE_HEADER_KERNEL_HERTZ:
		case PMID_FILE_HEADER_UNAME_RELEASE:
		case PMID_FILE_HEADER_UNAME_SYSNAME:
		case PMID_FILE_HEADER_UNAME_NODENAME:
		case PMID_FILE_HEADER_UNAME_MACHINE:
			pcp_read_file_header_stats(values, header);
			break;

		case PMID_RECORD_HEADER_KERNEL_UPTIME:
			pcp_read_record_header_stats(values, curr);
			break;

		case PMID_CPU_ALLCPU_USER:
		case PMID_CPU_ALLCPU_SYS:
		case PMID_CPU_ALLCPU_NICE:
		case PMID_CPU_ALLCPU_IDLE:
		case PMID_CPU_ALLCPU_WAITTOTAL:
		case PMID_CPU_ALLCPU_IRQTOTAL:
		case PMID_CPU_ALLCPU_IRQSOFT:
		case PMID_CPU_ALLCPU_IRQHARD:
		case PMID_CPU_ALLCPU_STEAL:
		case PMID_CPU_ALLCPU_GUEST:
		case PMID_CPU_ALLCPU_GUESTNICE:
		case PMID_CPU_PERCPU_USER:
		case PMID_CPU_PERCPU_NICE:
		case PMID_CPU_PERCPU_SYS:
		case PMID_CPU_PERCPU_IDLE:
		case PMID_CPU_PERCPU_WAITTOTAL:
		case PMID_CPU_PERCPU_IRQTOTAL:
		case PMID_CPU_PERCPU_IRQSOFT:
		case PMID_CPU_PERCPU_IRQHARD:
		case PMID_CPU_PERCPU_STEAL:
		case PMID_CPU_PERCPU_GUEST:
		case PMID_CPU_PERCPU_GUESTNICE:
		case PMID_CPU_PERCPU_INTERRUPTS:
			pcp_read_cpu_stats(values, act[A_CPU], curr);
			break;

		case PMID_POWER_PERCPU_CLOCK:
			pcp_read_pwr_cpufreq_stats(values, act[A_PWR_CPU], curr);
			break;

		case PMID_SOFTNET_ALLCPU_PROCESSED:
		case PMID_SOFTNET_ALLCPU_DROPPED:
		case PMID_SOFTNET_ALLCPU_TIMESQUEEZE:
		case PMID_SOFTNET_ALLCPU_RECEIVEDRPS:
		case PMID_SOFTNET_ALLCPU_FLOWLIMIT:
		case PMID_SOFTNET_ALLCPU_BACKLOGLENGTH:
		case PMID_SOFTNET_PERCPU_PROCESSED:
		case PMID_SOFTNET_PERCPU_DROPPED:
		case PMID_SOFTNET_PERCPU_TIMESQUEEZE:
		case PMID_SOFTNET_PERCPU_RECEIVEDRPS:
		case PMID_SOFTNET_PERCPU_FLOWLIMIT:
		case PMID_SOFTNET_PERCPU_BACKLOGLENGTH:
			pcp_read_softnet_stats(values, act[A_NET_SOFT], curr);
			break;

		case PMID_PCSW_CONTEXT_SWITCH:
		case PMID_PCSW_FORK_SYSCALLS:
			pcp_read_pcsw_stats(values, act[A_PCSW], curr);
			break;

		case PMID_IRQ_ALLIRQ_TOTAL:
		case PMID_IRQ_PERIRQ_TOTAL:
			pcp_read_irq_stats(values, act[A_IRQ], curr);
			break;

		case PMID_SWAP_PAGESIN:
		case PMID_SWAP_PAGESOUT:
			pcp_read_swap_stats(values, act[A_SWAP], curr);
			break;

		case PMID_PAGING_PGPGIN:
		case PMID_PAGING_PGPGOUT:
		case PMID_PAGING_PGFAULT:
		case PMID_PAGING_PGMAJFAULT:
		case PMID_PAGING_PGFREE:
		case PMID_PAGING_PGSCANDIRECT:
		case PMID_PAGING_PGSCANKSWAPD:
		case PMID_PAGING_PGSTEAL:
			pcp_read_paging_stats(values, act[A_PAGE], curr);
			break;

		case PMID_IO_ALLDEV_TOTAL:
		case PMID_IO_ALLDEV_READ:
		case PMID_IO_ALLDEV_WRITE:
		case PMID_IO_ALLDEV_DISCARD:
		case PMID_IO_ALLDEV_READBYTES:
		case PMID_IO_ALLDEV_WRITEBYTES:
		case PMID_IO_ALLDEV_DISCARDBYTES:
			pcp_read_io_stats(values, act[A_IO], curr);
			break;

		case PMID_MEM_PHYS_MB:
		case PMID_MEM_PHYS_KB:
		case PMID_MEM_UTIL_FREE:
		case PMID_MEM_UTIL_AVAIL:
		case PMID_MEM_UTIL_USED:
		case PMID_MEM_UTIL_BUFFER:
		case PMID_MEM_UTIL_CACHED:
		case PMID_MEM_UTIL_COMMITAS:
		case PMID_MEM_UTIL_ACTIVE:
		case PMID_MEM_UTIL_INACTIVE:
		case PMID_MEM_UTIL_DIRTY:
		case PMID_MEM_UTIL_ANON:
		case PMID_MEM_UTIL_SLAB:
		case PMID_MEM_UTIL_KSTACK:
		case PMID_MEM_UTIL_PGTABLE:
		case PMID_MEM_UTIL_VMALLOC:
		case PMID_MEM_UTIL_SWAPFREE:
		case PMID_MEM_UTIL_SWAPTOTAL:
		case PMID_MEM_UTIL_SWAPCACHED:
			pcp_read_memory_stats(values, act[A_MEMORY], curr);
			break;

		case PMID_KTABLE_DENTRYS:
		case PMID_KTABLE_FILES:
		case PMID_KTABLE_INODES:
		case PMID_KTABLE_PTYS:
			pcp_read_ktable_stats(values, act[A_KTABLES], curr);
			break;

		case PMID_KQUEUE_RUNNABLE:
		case PMID_KQUEUE_PROCESSES:
		case PMID_KQUEUE_BLOCKED:
		case PMID_KQUEUE_LOADAVG:
			pcp_read_kqueue_stats(values, act[A_QUEUE], curr);

		case PMID_DISK_PERDEV_READ:
		case PMID_DISK_PERDEV_WRITE:
		case PMID_DISK_PERDEV_TOTAL:
		case PMID_DISK_PERDEV_TOTALBYTES:
		case PMID_DISK_PERDEV_READBYTES:
		case PMID_DISK_PERDEV_WRITEBYTES:
		case PMID_DISK_PERDEV_DISCARDBYTES:
		case PMID_DISK_PERDEV_READACTIVE:
		case PMID_DISK_PERDEV_WRITEACTIVE:
		case PMID_DISK_PERDEV_TOTALACTIVE:
		case PMID_DISK_PERDEV_DISCARDACTIVE:
		case PMID_DISK_PERDEV_AVACTIVE:
		case PMID_DISK_PERDEV_AVQUEUE:
			pcp_read_disk_stats(values, act[A_DISK], curr);
			break;

		case PMID_NET_PERINTF_INPACKETS:
		case PMID_NET_PERINTF_OUTPACKETS:
		case PMID_NET_PERINTF_INBYTES:
		case PMID_NET_PERINTF_OUTBYTES:
		case PMID_NET_PERINTF_INCOMPRESS:
		case PMID_NET_PERINTF_OUTCOMPRESS:
		case PMID_NET_PERINTF_INMULTICAST:
			pcp_read_netdev_stats(values, act[A_NET_DEV], curr);
			break;

		case PMID_NET_EPERINTF_INERRORS:
		case PMID_NET_EPERINTF_OUTERRORS:
		case PMID_NET_EPERINTF_COLLISIONS:
		case PMID_NET_EPERINTF_INDROPS:
		case PMID_NET_EPERINTF_OUTDROPS:
		case PMID_NET_EPERINTF_OUTCARRIER:
		case PMID_NET_EPERINTF_INFRAME:
		case PMID_NET_EPERINTF_INFIFO:
		case PMID_NET_EPERINTF_OUTFIFO:
			pcp_read_enetdev_stats(values, act[A_NET_EDEV], curr);
			break;

		case PMID_SERIAL_PERTTY_RX:
		case PMID_SERIAL_PERTTY_TX:
		case PMID_SERIAL_PERTTY_FRAME:
		case PMID_SERIAL_PERTTY_PARITY:
		case PMID_SERIAL_PERTTY_BRK:
		case PMID_SERIAL_PERTTY_OVERRUN:
			pcp_read_serial_stats(values, act[A_SERIAL], curr);
			break;

		case PMID_SOCKET_TOTAL:
		case PMID_SOCKET_TCPINUSE:
		case PMID_SOCKET_UDPINUSE:
		case PMID_SOCKET_RAWINUSE:
		case PMID_SOCKET_FRAGINUSE:
		case PMID_SOCKET_TCPTW:
			pcp_read_net_sock_stats(values, act[A_NET_SOCK], curr);
			break;

		case PMID_NET_IP_INRECEIVES:
		case PMID_NET_IP_FORWDATAGRAMS:
		case PMID_NET_IP_INDELIVERS:
		case PMID_NET_IP_OUTREQUESTS:
		case PMID_NET_IP_REASMREQDS:
		case PMID_NET_IP_REASMOKS:
		case PMID_NET_IP_FRAGOKS:
		case PMID_NET_IP_FRAGCREATES:
			pcp_read_net_ip_stats(values, act[A_NET_IP], curr);
			break;

		case PMID_NET_EIP_INHDRERRORS:
		case PMID_NET_EIP_INADDRERRORS:
		case PMID_NET_EIP_INUNKNOWNPROTOS:
		case PMID_NET_EIP_INDISCARDS:
		case PMID_NET_EIP_OUTDISCARDS:
		case PMID_NET_EIP_OUTNOROUTES:
		case PMID_NET_EIP_REASMFAILS:
		case PMID_NET_EIP_FRAGFAILS:
			pcp_read_net_eip_stats(values, act[A_NET_EIP], curr);
			break;

		case PMID_NFSCLIENT_RPCCCNT:
		case PMID_NFSCLIENT_RPCRETRANS:
		case PMID_NFSCLIENT_REQUESTS:
			pcp_read_net_nfs_stats(values, act[A_NET_NFS], curr);
			break;

		case PMID_NFSSERVER_RPCCNT:
		case PMID_NFSSERVER_RPCBADCLNT:
		case PMID_NFSSERVER_NETCNT:
		case PMID_NFSSERVER_NETUDPCNT:
		case PMID_NFSSERVER_NETTCPCNT:
		case PMID_NFSSERVER_RCHITS:
		case PMID_NFSSERVER_RCMISSES:
		case PMID_NFSSERVER_REQUESTS:
			pcp_read_net_nfsd_stats(values, act[A_NET_NFSD], curr);
			break;

		case PMID_NET_ICMP_INMSGS:
		case PMID_NET_ICMP_OUTMSGS:
		case PMID_NET_ICMP_INECHOS:
		case PMID_NET_ICMP_INECHOREPS:
		case PMID_NET_ICMP_OUTECHOS:
		case PMID_NET_ICMP_OUTECHOREPS:
		case PMID_NET_ICMP_INTIMESTAMPS:
		case PMID_NET_ICMP_INTIMESTAMPREPS:
		case PMID_NET_ICMP_OUTTIMESTAMPS:
		case PMID_NET_ICMP_OUTTIMESTAMPREPS:
		case PMID_NET_ICMP_INADDRMASKS:
		case PMID_NET_ICMP_INADDRMASKREPS:
		case PMID_NET_ICMP_OUTADDRMASKS:
		case PMID_NET_ICMP_OUTADDRMASKREPS:
			pcp_read_net_icmp_stats(values, act[A_NET_ICMP], curr);
			break;

		case PMID_NET_EICMP_INERRORS:
		case PMID_NET_EICMP_OUTERRORS:
		case PMID_NET_EICMP_INDESTUNREACHS:
		case PMID_NET_EICMP_OUTDESTUNREACHS:
		case PMID_NET_EICMP_INTIMEEXCDS:
		case PMID_NET_EICMP_OUTTIMEEXCDS:
		case PMID_NET_EICMP_INPARMPROBS:
		case PMID_NET_EICMP_OUTPARMPROBS:
		case PMID_NET_EICMP_INSRCQUENCHS:
		case PMID_NET_EICMP_OUTSRCQUENCHS:
		case PMID_NET_EICMP_INREDIRECTS:
		case PMID_NET_EICMP_OUTREDIRECTS:
			pcp_read_net_eicmp_stats(values, act[A_NET_EICMP], curr);
			break;

		case PMID_NET_TCP_ACTIVEOPENS:
		case PMID_NET_TCP_PASSIVEOPENS:
		case PMID_NET_TCP_INSEGS:
		case PMID_NET_TCP_OUTSEGS:
			pcp_read_net_tcp_stats(values, act[A_NET_TCP], curr);
			break;

		case PMID_NET_ETCP_ATTEMPTFAILS:
		case PMID_NET_ETCP_ESTABRESETS:
		case PMID_NET_ETCP_RETRANSSEGS:
		case PMID_NET_ETCP_INERRS:
		case PMID_NET_ETCP_OUTRSTS:
			pcp_read_net_etcp_stats(values, act[A_NET_ETCP], curr);
			break;

		case PMID_NET_UDP_INDATAGRAMS:
		case PMID_NET_UDP_OUTDATAGRAMS:
		case PMID_NET_UDP_NOPORTS:
		case PMID_NET_UDP_INERRORS:
			pcp_read_net_udp_stats(values, act[A_NET_UDP], curr);
			break;

		case PMID_NET_SOCK6_TCPINUSE:
		case PMID_NET_SOCK6_UDPINUSE:
		case PMID_NET_SOCK6_RAWINUSE:
		case PMID_NET_SOCK6_FRAGINUSE:
			pcp_read_net_sock6_stats(values, act[A_NET_SOCK6], curr);
			break;

		case PMID_NET_IP6_INRECEIVES:
		case PMID_NET_IP6_OUTFORWDATAGRAMS:
		case PMID_NET_IP6_INDELIVERS:
		case PMID_NET_IP6_OUTREQUESTS:
		case PMID_NET_IP6_REASMREQDS:
		case PMID_NET_IP6_REASMOKS:
		case PMID_NET_IP6_INMCASTPKTS:
		case PMID_NET_IP6_OUTMCASTPKTS:
		case PMID_NET_IP6_FRAGOKS:
		case PMID_NET_IP6_FRAGCREATES:
			pcp_read_net_ip6_stats(values, act[A_NET_IP6], curr);
			break;

		case PMID_NET_EIP6_INHDRERRORS:
		case PMID_NET_EIP6_INADDRERRORS:
		case PMID_NET_EIP6_INUNKNOWNPROTOS:
		case PMID_NET_EIP6_INTOOBIGERRORS:
		case PMID_NET_EIP6_INDISCARDS:
		case PMID_NET_EIP6_OUTDISCARDS:
		case PMID_NET_EIP6_INNOROUTES:
		case PMID_NET_EIP6_OUTNOROUTES:
		case PMID_NET_EIP6_REASMFAILS:
		case PMID_NET_EIP6_FRAGFAILS:
		case PMID_NET_EIP6_INTRUNCATEDPKTS:
			pcp_read_net_eip6_stats(values, act[A_NET_EIP6], curr);
			break;

		case PMID_NET_ICMP6_INMSGS:
		case PMID_NET_ICMP6_OUTMSGS:
		case PMID_NET_ICMP6_INECHOS:
		case PMID_NET_ICMP6_INECHOREPLIES:
		case PMID_NET_ICMP6_OUTECHOREPLIES:
		case PMID_NET_ICMP6_INGROUPMEMBQUERIES:
		case PMID_NET_ICMP6_INGROUPMEMBRESPONSES:
		case PMID_NET_ICMP6_OUTGROUPMEMBRESPONSES:
		case PMID_NET_ICMP6_INGROUPMEMBREDUCTIONS:
		case PMID_NET_ICMP6_OUTGROUPMEMBREDUCTIONS:
		case PMID_NET_ICMP6_INROUTERSOLICITS:
		case PMID_NET_ICMP6_OUTROUTERSOLICITS:
		case PMID_NET_ICMP6_INROUTERADVERTISEMENTS:
		case PMID_NET_ICMP6_INNEIGHBORSOLICITS:
		case PMID_NET_ICMP6_OUTNEIGHBORSOLICITS:
		case PMID_NET_ICMP6_INNEIGHBORADVERTISEMENTS:
		case PMID_NET_ICMP6_OUTNEIGHBORADVERTISEMENTS:
			pcp_read_net_icmp6_stats(values, act[A_NET_ICMP6], curr);
			break;

		case PMID_NET_EICMP6_INERRORS:
		case PMID_NET_EICMP6_INDESTUNREACHS:
		case PMID_NET_EICMP6_OUTDESTUNREACHS:
		case PMID_NET_EICMP6_INTIMEEXCDS:
		case PMID_NET_EICMP6_OUTTIMEEXCDS:
		case PMID_NET_EICMP6_INPARMPROBLEMS:
		case PMID_NET_EICMP6_OUTPARMPROBLEMS:
		case PMID_NET_EICMP6_INREDIRECTS:
		case PMID_NET_EICMP6_OUTREDIRECTS:
		case PMID_NET_EICMP6_INPKTTOOBIGS:
		case PMID_NET_EICMP6_OUTPKTTOOBIGS:
			pcp_read_net_eicmp6_stats(values, act[A_NET_EICMP6], curr);
			break;

		case PMID_NET_UDP6_INDATAGRAMS:
		case PMID_NET_UDP6_OUTDATAGRAMS:
		case PMID_NET_UDP6_NOPORTS:
		case PMID_NET_UDP6_INERRORS:
			pcp_read_net_udp6_stats(values, act[A_NET_UDP6], curr);
			break;

		case PMID_MEM_HUGE_TOTALBYTES:
		case PMID_MEM_HUGE_FREEBYTES:
		case PMID_MEM_HUGE_RSVDBYTES:
		case PMID_MEM_HUGE_SURPBYTES:
			pcp_read_huge_stats(values, act[A_HUGE], curr);
			break;

		case PMID_POWER_FAN_RPM:
		case PMID_POWER_FAN_DRPM:
		case PMID_POWER_FAN_DEVICE:
			pcp_read_power_fan_stats(values, act[A_PWR_FAN], curr);
			break;

		case PMID_POWER_TEMP_CELSIUS:
		case PMID_POWER_TEMP_PERCENT:
		case PMID_POWER_TEMP_DEVICE:
			pcp_read_power_temp_stats(values, act[A_PWR_TEMP], curr);
			break;

		case PMID_POWER_IN_VOLTAGE:
		case PMID_POWER_IN_PERCENT:
		case PMID_POWER_IN_DEVICE:
			pcp_read_power_in_stats(values, act[A_PWR_IN], curr);
			break;

		case PMID_POWER_BAT_CAPACITY:
		case PMID_POWER_BAT_STATUS:
			pcp_read_power_bat_stats(values, act[A_PWR_BAT], curr);
			break;

		case PMID_POWER_USB_BUS:
		case PMID_POWER_USB_VENDORID:
		case PMID_POWER_USB_PRODUCTID:
		case PMID_POWER_USB_MAXPOWER:
		case PMID_POWER_USB_MANUFACTURER:
		case PMID_POWER_USB_PRODUCTNAME:
			pcp_read_power_usb_stats(values, act[A_PWR_USB], curr);
			break;

		case PMID_FILESYS_CAPACITY:
		case PMID_FILESYS_FREE:
		case PMID_FILESYS_USED:
		case PMID_FILESYS_FULL:
		case PMID_FILESYS_MAXFILES:
		case PMID_FILESYS_FREEFILES:
		case PMID_FILESYS_USEDFILES:
		case PMID_FILESYS_AVAIL:
			pcp_read_filesystem_stats(values, act[A_FS], curr);
			break;

		case PMID_FCHOST_INFRAMES:
		case PMID_FCHOST_OUTFRAMES:
		case PMID_FCHOST_INBYTES:
		case PMID_FCHOST_OUTBYTES:
			pcp_read_fchost_stats(values, act[A_NET_FC], curr);
			break;

		case PMID_PSI_CPU_SOMETOTAL:
		case PMID_PSI_CPU_SOMEAVG:
			pcp_read_psicpu_stats(values, act[A_PSI_CPU], curr);
			break;

		case PMID_PSI_IO_SOMETOTAL:
		case PMID_PSI_IO_SOMEAVG:
		case PMID_PSI_IO_FULLTOTAL:
		case PMID_PSI_IO_FULLAVG:
			pcp_read_psiio_stats(values, act[A_PSI_IO], curr);
			break;

		case PMID_PSI_MEM_SOMETOTAL:
		case PMID_PSI_MEM_SOMEAVG:
		case PMID_PSI_MEM_FULLTOTAL:
		case PMID_PSI_MEM_FULLAVG:
			pcp_read_psimem_stats(values, act[A_PSI_MEM], curr);
			break;
	}
}
