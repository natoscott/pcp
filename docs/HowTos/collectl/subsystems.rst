.. include:: ../../refs.rst

Subsystem Reference
###################

This page documents the metrics collected by each pcp-collectl subsystem,
the PCP metric names they map to, and the output column layout.

CPU (``c`` / ``C``)
********************

Brief mode (``-s c``) — one summary line per interval::

    #Time       User%  Nice%   Sys%  Wait%   IRQ%  Soft%  Steal%  Busy%

Detail mode (``-s C``) — one line per CPU, plus frequency::

    #Time CPU  User%  Nice%   Sys%  Wait%   IRQ%  Soft%  Steal%  Busy%   MHz

PCP metrics: ``kernel.all.cpu.*``, ``kernel.percpu.cpu.*``,
``hinv.cpu.frequency_scaling.current``

Disk (``d`` / ``D``)
*********************

Brief mode — summary across all devices::

    #Time       KBRead   KBWrit  Reads  Writes

Detail mode — one line per device::

    #Time Disk    KBRead   KBWrit  Reads  Writes  RdMrg  WrMrg  Busy%

Filter devices with ``--dskfilt``.

PCP metrics: ``disk.all.*``, ``disk.dev.*``

Memory (``m``)
**************

::

    #Time          Used   Buff   Cach  Dirty  Inact   Slab    Map

All values in KB.  PCP metrics: ``mem.util.{used,bufmem,cached,dirty,inactive,slab,mapped}``

NUMA Memory (``M``)
********************

One line per NUMA node::

    #Time Node   Total    Used    Free  Active  Inact

PCP metrics: ``mem.numa.util.*`` (requires NUMA hardware)

Network (``n`` / ``N``)
************************

Brief — aggregate across all interfaces::

    #Time       RxKB   TxKB  RxPkt  TxPkt  RxErr  TxErr  RxDrp  TxDrp

Detail — per interface::

    #Time Iface    RxKB   TxKB  RxPkt  TxPkt  RxErr  TxErr  RxDrp  TxDrp

Filter interfaces with ``--netfilt``.

PCP metrics: ``network.all.*``, ``network.interface.*``

Sockets (``s``)
***************

::

    #Time    Tot TCPIn  Orph TWait UDPIn  Raw Frag

PCP metrics: ``network.sockstat.*``

TCP (``t``)
***********

::

    #Time  ActOpn PasOpn CurrEst Retrans  InErr OutRst

PCP metrics: ``network.tcp.*``, ``network.tcpconn.*``

Inodes / Files (``i``)
***********************

::

    #Time  Dentries  Files  Inodes

PCP metrics: ``vfs.dentry.count``, ``vfs.files.count``, ``vfs.inodes.count``

Interrupts (``j`` / ``J``)
****************************

Brief — total interrupts per second::

    #Time    Intrpts

Detail — per CPU per interrupt type (filtered by ``--intfilt``).

PCP metrics: ``kernel.all.intr``, ``kernel.percpu.interrupts.*``

Slab Allocator (``y`` / ``Y``)
*******************************

Detail — per slab entry::

    #Time Slab  ActObj  TotObj  ActSlb  TotSlb

PCP metrics: ``mem.slabinfo.*``

Buddy / Fragmentation (``b``)
*******************************

Memory fragmentation state per zone and order::

    #Time Zone       TotKB  Pages

PCP metrics: ``mem.buddyinfo.{total,pages}``

NFS (``f``)
***********

Client and server RPC call counts::

    #Time  Calls V3Call V4Call

PCP metrics: ``nfs.client.*``, ``nfs3.client.*``, ``nfs4.client.*``

Process (``Z``)
***************

One line per process, sorted by CPU%::

    #Time    PID  CPU%  RssMB  VszMB  Thr  Command

Filter processes with ``--procfilt``.  Collection interval controlled by
``-i primary:secondary`` (default secondary interval: 60s).

PCP metrics: ``proc.psinfo.*``, ``proc.memory.*``, ``proc.io.*``
