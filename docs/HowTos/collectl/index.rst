.. include:: ../../refs.rst

pcp-collectl User Guide
#######################

`pcp-collectl` is a PCP-native C reimplementation of the Perl `collectl` utility.
It collects the same metrics as the original but uses PCP (Performance Co-Pilot)
for data collection and writes native PCP archives rather than collectl raw files.

As a compiled C program it has lower CPU and memory overhead than the
original Perl implementation, while integrating cleanly with the PCP
ecosystem — archives can be replayed with any PCP tool (``pmval``,
``pmrep``, ``pmlogger``, ``pcp-atop``, etc.).

The original collectl project is at http://collectl.sourceforge.net/

.. toctree::
   :maxdepth: 2

   getting-started
   subsystems
   archives
   colmux


Quick Start
***********

Install and start collecting immediately::

    $ pcp-collectl -s cdn -i 5

This monitors CPU (``c``), disk (``d``), and network (``n``) at 5-second
intervals, printing brief one-line summaries to the terminal.

For daemon mode writing PCP archives (rotated daily, 7-day retention)::

    $ pcp-collectl -D -f /var/log/collectl -r 00:00,7

Replay a recorded archive::

    $ pcp-collectl -a /var/log/collectl/hostname-20260620

Key Differences from Original collectl
***************************************

* **Output**: PCP archives (not collectl raw files). Convert existing
  raw archives with ``collectl2pcp(1)``.
* **Collection**: Via ``pmFetch()`` from linux PMDA DSOs — no direct
  ``/proc`` reads.
* **Compression**: ``.meta`` → xz, data volumes → zstd after rotation.
  No dependency on ``pmlogger_daily``.
* **Playback**: ``-a`` / ``-p`` opens PCP archives; any PCP tool can
  also read the same archives directly.
* **New options**: ``-a/--archive`` (alias for ``-p``), ``-S/--start``,
  ``-T/--finish`` for time window selection.
