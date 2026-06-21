.. include:: ../../refs.rst

Archive Management
##################

pcp-collectl writes native PCP archives (``.meta``, ``.index``, ``.N`` data
volumes).  These archives can be read by any PCP tool, not just
pcp-collectl itself.

Writing Archives
*****************

Specify the output directory with ``-f``::

    $ pcp-collectl -s cdmn -f /var/log/collectl

Archives are named ``hostname-YYYYMMDD`` within the directory — for example
``fedora-20260621``.  Each day gets a fresh archive via append mode; no new
file is opened unless a rotation is triggered.

Daemon Mode
************

``-D`` daemonises pcp-collectl and uses the default subsystem set for
long-running collection::

    $ pcp-collectl -D -f /var/log/collectl

The ``collectl.service`` systemd unit does this automatically on boot.
Customise the default DaemonCommands in ``/etc/pcp/collectl/collectl.conf``
to change subsystems, interval, or retention.

Log Rotation
*************

The ``-r HH:MM[,purgeDays[,rollMins]]`` option controls rotation::

    # Roll at midnight, keep 14 days
    $ pcp-collectl -D -f /var/log/collectl -r 00:00,14

    # Roll every 6 hours, keep 7 days
    $ pcp-collectl -D -f /var/log/collectl -r 00:00,7,360

At each rotation:

1. The current archive is closed.
2. ``.meta`` files are compressed with **xz** (high ratio; metadata read
   once at archive open).
3. Data volumes (``.0``, ``.1``, ...) are compressed with **zstd** (fast
   decompression for replay with ``pmval``/``pmrep``).
4. ``.index`` files are **not** compressed (tiny; required for fast seeks).
5. Archives older than ``purgeDays`` are deleted from the directory.

Compression runs in a background child process so collection continues
uninterrupted.

.. note::

   pcp-collectl manages its own archives and does not depend on
   ``pmlogger_daily`` or any other PCP service.  Package dependencies
   are ``pcp-libs`` and ``pcp-conf`` only.

Trigger an immediate manual rotation with ``SIGUSR1``::

    $ kill -USR1 $(cat /run/collectl.pid)

Migrating Existing collectl Raw Archives
*****************************************

Existing ``.raw`` / ``.raw.gz`` archives from the original collectl can be
converted to PCP format using ``collectl2pcp``::

    $ collectl2pcp /var/log/collectl/hostname-20260101.raw.gz /tmp/old-arch

The resulting PCP archive can then be replayed with pcp-collectl or any
other PCP tool.

.. note::

   pcp-collectl does **not** write collectl raw format.  Use
   ``collectl2pcp(1)`` to migrate existing raw archives.

Playback
*********

Replay a PCP archive written by pcp-collectl::

    # Same output as live mode but from archive
    $ pcp-collectl -a /var/log/collectl/fedora-20260620

    # -p is also accepted (original collectl flag)
    $ pcp-collectl -p /var/log/collectl/fedora-20260620

    # Time window
    $ pcp-collectl -a /var/log/collectl/fedora-20260620 \
        -S 10:00 -T 11:00

All subsystem output functions are shared between live and playback
modes — display looks identical.

Because the archives are standard PCP format, any PCP tool can also
read them directly::

    $ pmval -a /var/log/collectl/fedora-20260620 kernel.all.cpu.user
    $ pmrep -a /var/log/collectl/fedora-20260620 disk.all.read_bytes
    $ pcp-atop -r /var/log/collectl/fedora-20260620
