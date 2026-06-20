# pcp-collectl — Implementation Context

## What this is

A full C reimplementation of the collectl and colmux monitoring tools, written
as a native PCP tool under src/pcp/collectl/. Branch: pcp-collectl (forked from
pmi-append-mode in ~/git/pcp; worktree at ~/git/pcp-collectl).

## Implementation plan

The full plan is at: ~/.claude/plans/typed-wishing-castle.md — read it first.

## Key design decisions

- **No raw format output** — PCP archives only (libpcp_import + PMI_APPEND)
- **Table-driven subsystem architecture** — two files handle all subsystems:
  - subsys.c: generic pmFetch/rate-calc/output/archive machinery
  - subsys_defs.c: table of metric_spec + subsys_def for every subsystem code
- **Collection via pmFetch only** — no direct /proc reading; linux PMDA for all metrics
- **One new linux PMDA metric needed**: hinv.cpu.frequency_scaling.current
  (in src/pmdas/linux/proc_cpuinfo.c, reads scaling_cur_freq, alongside existing
  .min/.max/.count/.time metrics)
- **collectl2pcp gaps to fill**: socket, inode, slab, buddy, NFS, NUMA, irq detail
  handlers missing from src/collectl2pcp/collectl2pcp.c
- **Symlinks** (collectl→pcp-collectl, colmux→pcp-colmux) via --with-collectl-symlink
  configure option, same pattern as --with-dstat-symlink
- **Config**: /etc/pcp/collectl/collectl.conf (primary), symlink from /etc/collectl/
- **Systemd service**: service/pcp-collectl.service.in with @PCP_BIN_DIR@/@PCP_RUN_DIR@
- **New CLI options** (added, no conflicts): -a/--archive (alias -p), -S/--start
  (alias --from), -T/--finish (alias --thru)
- **QA tests**: 2100-2105 range
- **Packaging**: Replaces/Obsoletes: collectl; URL: http://collectl.sourceforge.net/

## Source of truth for original tool

~/git/collectl/collectl   (6854-line Perl script)
~/git/collectl/colmux     (2395-line Perl script)
~/git/collectl/man1/      (original man pages — start from these)

## Reuse from ~/git/sysstat

- pcp_local.c:pcp_local_write_pmid_help() — write metric help text into archive
- pcp_stats.c:pcp_read_u64/u32() — value extraction helpers
- pcp_def_metrics.h:PMI_ID/PMI_INDOM macros — copy into pcp-collectl.h
- sadc.c daemonisation + interval alignment patterns

## Security audit

Once implementation is complete, spawn a fresh security-review subagent with
no prior context to audit: input validation, socket handling (port 2655),
file path construction, privilege dropping (--runas), all string operations.

## Author / acknowledgements

Original author: Mark Seger (mjseger@gmail.com), HP.
Keep all SourceForge URL references in man pages.
Add to ACKNOWLEDGEMENTS: "The PCP-native C reimplementation was contributed
by Red Hat, Inc. Special thanks to Mark Seger for creating collectl."
