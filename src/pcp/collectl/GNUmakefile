#
# Copyright (c) 2026 Red Hat.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

CMDTARGETS = pcp-collectl

CFILES = pcp-collectl.c \
	 subsys.c subsys_defs.c \
	 collect.c playback.c \
	 daemon.c socket.c \
	 filter.c archive.c output.c

HFILES = pcp-collectl.h

LLDLIBS = $(PCP_LIB) -lpcp_import -lm

# Parameterised paths injected at build time
LCFLAGS += -DCOLLECTL_SYSCONF_PATH='"$(PCP_SYSCONF_DIR)"'
LCFLAGS += -DCOLLECTL_RUN_PATH='"$(PCP_RUN_DIR)"'
LCFLAGS += -DCOLLECTL_LOG_PATH='"$(PCP_LOG_DIR)"'
LCFLAGS += -DPCP_COLLECTL_VERSION='"4.3.10"'

LDIRT = collectl.service collectl.conf

default: $(CMDTARGETS) collectl.service collectl.conf

# .in -> final: substitute @PCP_*@ placeholders
collectl.service: service/collectl.service.in
	$(SED) \
	    -e 's|@PCP_BINADM_DIR@|$(PCP_BINADM_DIR)|g' \
	    -e 's|@PCP_RUN_DIR@|$(PCP_RUN_DIR)|g' \
	    $< > $@

collectl.conf: collectl.conf.in
	$(SED) \
	    -e 's|@PCP_LOG_DIR@|$(PCP_LOG_DIR)|g' \
	    -e 's|@PCP_SYSCONF_DIR@|$(PCP_SYSCONF_DIR)|g' \
	    $< > $@

install: default
	$(INSTALL) -m 755 pcp-collectl $(PCP_BINADM_DIR)/pcp-collectl
	$(INSTALL) -m 755 -d $(PCP_SYSCONF_DIR)/collectl
	$(INSTALL) -m 644 collectl.conf $(PCP_SYSCONF_DIR)/collectl/collectl.conf
	$(INSTALL) -m 644 collectl.service $(PCP_SYSTEMDUNIT_DIR)/collectl.service
	$(INSTALL) -m 644 man/pcp-collectl.1 $(PCP_MAN_DIR)/man1/pcp-collectl.1
	$(INSTALL) -S $(PCP_BINADM_DIR)/pcp-collectl $(PCP_BIN_DIR)/collectl
	$(INSTALL) -S $(PCP_MAN_DIR)/man1/pcp-collectl.1 $(PCP_MAN_DIR)/man1/collectl.1

include $(BUILDRULES)
