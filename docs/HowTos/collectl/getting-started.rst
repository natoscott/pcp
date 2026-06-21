.. include:: ../../refs.rst

Getting Started
###############

Subsystem Selection (``-s``)
*****************************

Subsystems are selected by single-letter codes passed to ``-s``.
Lowercase letters select summary data; uppercase select per-instance
detail.  Codes can be combined (``-s cdn``) or modified with ``+``/``-``
(``-s bcdfijmnstx+Z``).

.. list-table:: Subsystem Codes
   :header-rows: 1
   :widths: 10 10 80

   * - Code
     - Detail
     - Description
   * - ``b``
     - —
     - Buddy/fragmentation info (``mem.buddyinfo.*``)
   * - ``c``
     - ``C``
     - CPU summary / per-CPU (``kernel.all.cpu.*``, ``kernel.percpu.cpu.*``)
   * - ``d``
     - ``D``
     - Disk summary / per-device (``disk.all.*``, ``disk.dev.*``)
   * - ``E``
     - —
     - Environmental sensors via IPMI PMDA (optional)
   * - ``f``
     - ``F``
     - NFS client/server (``nfs.client.*``, ``nfs.server.*``)
   * - ``i``
     - —
     - Inodes, dentries, file handles (``vfs.*.count``)
   * - ``j``
     - ``J``
     - Interrupt summary / per-CPU (``kernel.all.intr``, ``kernel.percpu.interrupts.*``)
   * - ``l``
     - —
     - Lustre filesystem stats (optional — requires Lustre PMDA)
   * - ``m``
     - ``M``
     - Memory summary / NUMA (``mem.util.*``, ``mem.numa.util.*``)
   * - ``n``
     - ``N``
     - Network summary / per-interface (``network.all.*``, ``network.interface.*``)
   * - ``s``
     - —
     - Socket counts (``network.sockstat.*``)
   * - ``t``
     - ``T``
     - TCP summary / detail (``network.tcp.*``, ``network.tcpconn.*``)
   * - ``x``
     - ``X``
     - InfiniBand (optional — requires IB PMDA)
   * - ``y``
     - ``Y``
     - Slab allocator (``mem.slabinfo.*``)
   * - ``Z``
     - —
     - Processes (``proc.psinfo.*``, ``proc.memory.*``, ``proc.io.*``)

Default subsystems:

* **Interactive mode**: ``cdn`` (CPU, disk, network)
* **Daemon mode** (``-D``): ``bcdfijmnstx`` (all core subsystems)

Output Modes
*************

Brief mode (default)
  One line per subsystem per interval.  Numbers scaled to K/M/G unless
  ``-w`` (wide) is specified.  Header every 25 lines (``--hr N`` to change)::

    #Time       User%  Nice%   Sys%  Wait%   IRQ%  Soft%  Steal%  Busy%
    10:30:00      1.2    0.0    0.4    0.0    0.0    0.1    0.0    1.7

Verbose mode (``--verbose``)
  Multi-line output with one row per instance for detail subsystems::

    # SINGLE CPU STATISTICS
    #Time CPU  User% Nice%  Sys% Wait%  IRQ% Soft% Steal%  Busy%
    10:30:00   0     0.8   0.0   0.3   0.0   0.0   0.1   0.0    1.2

Understanding the Numbers
*************************

Counter metrics (disk I/O, network packets, interrupts) are displayed
as **rates per second** over the collection interval — the same
normalisation as ``sar``.  Memory and CPU percentage metrics are
instantaneous.

The formula for counter metrics is::

    rate = (current_value - previous_value) / elapsed_seconds

This means the first sample after startup shows 0 for counter-based
metrics (no previous value to subtract).

Instance Filtering
*******************

Per-instance subsystems (disks, networks, CPUs) can be filtered::

    # Show only sda and nvme devices
    $ pcp-collectl -s D --dskfilt 'sda|nvme'

    # Show only eth interfaces
    $ pcp-collectl -s N --netfilt eth

    # Exclude loopback
    $ pcp-collectl -s N --netfilt '^lo'  # ^ prefix = exclude

The pattern is a POSIX extended regex applied to instance names.
``^pattern`` excludes matching instances; any other pattern includes
only matching instances.

Common Examples
***************

::

    # CPU, disk, memory, network — 5 second intervals
    $ pcp-collectl -s cdmn -i 5

    # All core subsystems, daemon mode, 7-day retention
    $ pcp-collectl -D -s bcdfijmnst -f /var/log/collectl -r 00:00,7

    # Process monitoring alongside core subsystems
    $ pcp-collectl -s cdmnZ -i 5:60

    # Wide output (no K/M/G scaling)
    $ pcp-collectl -s d -w

    # Verbose with per-CPU breakdown
    $ pcp-collectl -s cC --verbose

    # Count 10 samples at 2-second intervals then exit
    $ pcp-collectl -s cn -i 2 -c 10
