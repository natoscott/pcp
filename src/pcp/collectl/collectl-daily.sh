#!/bin/sh
#
# collectl-daily - daily maintenance for pcp-collectl PCP archives
#
# Reads $PCP_ETC_DIR/collectl.conf to determine the archive directory
# (from DaemonCommands -f) and retention period (from DaemonCommands -r).
#
# Usage:
#   collectl-daily [--compress-only] [LOGPATH]
#
#   --compress-only  Compress prior-day archives only; skip culling.
#   LOGPATH          Override the archive directory from collectl.conf.
#
# Without --compress-only (normal daily run via timer or ExecStartPre):
#   - Creates LOGPATH if absent
#   - Compresses prior-day archives (.meta -> xz, volumes -> zstd)
#   - Culls archives older than the configured retention period
#
# Copyright 2003-2018 Hewlett-Packard Development Company, L.P.
# Copyright (c) 2026 Red Hat.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#

. ${PCP_DIR}/etc/pcp.env

CONFFILE=$PCP_ETC_DIR/collectl.conf
compress_only=false

for arg in "$@"
do
    case "$arg" in
    --compress-only)
        compress_only=true
        ;;
    -*)
        echo "$0: unknown option: $arg" >&2
        exit 1
        ;;
    *)
        LOGPATH="$arg"
        ;;
    esac
done

#
# Parse DaemonCommands from collectl.conf to extract -f and -r values.
#
if [ -r "$CONFFILE" ] && [ -z "$LOGPATH" ]
then
    daemon_cmd=$(sed -n 's/^DaemonCommands[[:space:]]*=[[:space:]]*//p' "$CONFFILE")
    set -- $daemon_cmd
    while [ $# -gt 0 ]
    do
        case "$1" in
        -f) LOGPATH="$2"; shift ;;
        -r) # -r HH:MM,purgeDays[,rollMins]
            purge=$(echo "$2" | cut -d, -f2)
            [ -n "$purge" ] && LOGGENERATIONS="$purge"
            shift ;;
        esac
        shift
    done
fi

: ${LOGPATH:=/var/log/collectl}
: ${LOGGENERATIONS:=7}

$compress_only || mkdir -p "$LOGPATH" || exit 1

today=$(date +%Y%m%d)

#
# Atomically rename prior-day archive files to .tmp staging names, then
# fork background compression for each.  The rename is safe whether
# pcp-collectl is running or stopped.
#
for metafile in "$LOGPATH"/*-[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9].meta
do
    [ -f "$metafile" ] || continue
    base="${metafile%.meta}"
    day="${base##*-}"
    [ "$day" = "$today" ] && continue

    mv -f "$metafile" "$metafile.tmp" 2>/dev/null || continue

    for vol in "$base".[0-9]*
    do
        [ -f "$vol" ] && mv -f "$vol" "$vol.tmp" 2>/dev/null
    done
done

for f in "$LOGPATH"/*.meta.tmp
do
    [ -f "$f" ] || continue
    out="${f%.tmp}.xz"
    (xz -q -c "$f" > "$out" && rm -f "$f") &
done

for f in "$LOGPATH"/*.[0-9]*.tmp
do
    [ -f "$f" ] || continue
    out="${f%.tmp}.zst"
    (zstd -q -o "$out" "$f" && rm -f "$f") &
done

wait

$compress_only && exit 0

#
# Cull archives by the YYYYMMDD date embedded in the filename.
# LOGGENERATIONS=0 means keep forever.
#
if [ "${LOGGENERATIONS:-0}" -gt 0 ]
then
    cutdate=$(date -d "-${LOGGENERATIONS} days" +%Y%m%d 2>/dev/null || \
              date -v"-${LOGGENERATIONS}d" +%Y%m%d 2>/dev/null)
    if [ -n "$cutdate" ]
    then
        for f in "$LOGPATH"/*-[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9].*
        do
            [ -f "$f" ] || continue
            base="${f##*/}"
            day="${base#*-}"; day="${day%%.*}"
            [ "$day" -lt "$cutdate" ] 2>/dev/null && rm -f "$f"
        done
    fi
fi

exit 0
