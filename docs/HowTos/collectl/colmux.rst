.. include:: ../../refs.rst

pcp-colmux: Multi-Host Monitoring
###################################

``pcp-colmux`` aggregates monitoring data from multiple systems running
``pcp-collectl`` and displays it as a sorted table or columnar output.

.. note::

   ``pcp-colmux`` uses PCP-native connections (``pmcd`` on each remote
   host) as the primary transport.  The original collectl TCP socket
   protocol on port 2655 is also supported for **rolling upgrades** where
   some remote hosts still run the original Perl collectl — but this mode
   is deprecated.  Upgrade remote hosts to ``pcp-collectl`` to eliminate
   the dependency on the legacy protocol.

Connection
***********

Specify hosts with ``-address``::

    # Single host
    $ pcp-colmux -address host1 -command "-s cdn -i 10"

    # Comma-separated list
    $ pcp-colmux -address host1,host2,host3 -command "-s c"

    # PDSH range expansion
    $ pcp-colmux -address "n[1-10]" -command "-s cdn"

    # Mixed ranges and single hosts
    $ pcp-colmux -address "web[01-05],db1,db2" -command "-s cmn"

    # Hosts from a file (one per line)
    $ pcp-colmux -address @/etc/monitoring/hosts -command "-s cdn"

The ``-command`` string is the set of pcp-collectl options describing which
metrics to fetch from each host.

Multi-Line Sorted Display (default)
*************************************

One row per host, sorted by a selected column::

    Host       User%  Nice%   Sys%  Wait%  Busy%
    web01        5.2    0.0    1.4    0.0    6.6
    web02        4.8    0.0    1.2    0.0    6.0
    db1         12.3    0.0    3.1    0.1   15.5

Interactive controls (when stdout is a terminal):

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Key
     - Action
   * - Left/Right arrow
     - Change sort column
   * - Up/Down arrow
     - Toggle sort direction (ascending/descending)
   * - ``r``
     - Toggle reverse sort
   * - ``z``
     - Toggle zero-row filtering
   * - ``f``
     - Freeze sort order (stop auto-sorting)
   * - ``q`` / Ctrl-C
     - Quit

Options::

    -column N       Sort by column N (0-based; default: 0)
    -reverse        Sort descending
    -zero           Hide rows where sort column is zero
    -hostwidth N    Hostname column width (default: 8)
    -lines N        Max rows to display (default: terminal height)
    -noescape       Disable VT100 escape sequences (plain text output)
    -nobold         Disable bold highlighting of sort column

Columnar Display (``-cols``)
******************************

Display specific columns horizontally, one value per host per column::

    $ pcp-colmux -address "n[1-10]" -command "-s c" -cols 0,2,4

Options::

    -cols N[,N,...]   Select columns by number (use -colhelp to list)
    -colhelp          Show column numbers and headings
    -coltotal         Append column totals at the right
    -colwidth N       Column width (default: 6)
    -col1000          Divide all column values by 1000
    -colk             Divide all column values by 1024
    -collog10         Apply log10() transformation

Playback
*********

Replay archives from multiple hosts::

    # Single archive filespec (applied to all hosts)
    $ pcp-colmux -address "n[1-5]" \
        -command "-s cdn -p /var/log/collectl/HOST-20260620"

    # Glob pattern — one archive per host matched in order
    $ pcp-colmux -address "n[1-5]" \
        -command "-s cdn -p '/var/log/collectl/n*-20260620'"

    # Add a delay between intervals during playback
    $ pcp-colmux -address "n[1-5]" \
        -command "-s cdn -p /var/log/collectl/n1-20260620" \
        -delay 0.5
