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

CMDTARGET = pcp-collectl
MUXTARGET = pcp-colmux
CMDTARGETS = $(CMDTARGET) $(MUXTARGET)

CFILES = pcp-collectl.c \
	 subsys.c subsys_defs.c \
	 collect.c playback.c \
	 daemon.c socket.c \
	 filter.c archive.c output.c

COLMUX_CFILES = pcp-colmux.c \
	pdsh.c mux.c display.c \
	colmux-socket.c colmux-playback.c \
	subsys.c subsys_defs.c

HFILES = pcp-collectl.h pcp-colmux.h

LLDLIBS = $(PCPLIB) -lpcp_archive -lpcp_import $(LIB_FOR_MATH)

COLLECTL_OBJECTS = $(CFILES:.c=.o)
COLMUX_OBJECTS   = $(COLMUX_CFILES:.c=.o)

LDIRT = $(COLMUX_OBJECTS) $(MUXTARGET)

LCFLAGS += -DCOLLECTL_SYSCONF_PATH='"$(PCP_SYSCONF_DIR)"'
LCFLAGS += -DCOLLECTL_LOG_PATH='"/var/log/collectl"'
LCFLAGS += -DPCP_COLLECTL_VERSION='"4.3.20"'

MAN_SECTION = 1
MAN_PAGES = $(CMDTARGET).$(MAN_SECTION) $(MUXTARGET).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default: build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: $(CMDTARGETS)

$(MUXTARGET) : $(COLMUX_OBJECTS)
	$(CCF) -o $(MUXTARGET) $(LDFLAGS) $(COLMUX_OBJECTS) $(LDLIBS)

install: default
	$(INSTALL) -m 755 pcp-collectl pcp-colmux $(PCP_BINADM_DIR)
	$(INSTALL) -m 755 collectl-daily.sh $(PCP_BINADM_DIR)/collectl-daily
	$(INSTALL) -m 644 collectl.conf $(PCP_ETC_DIR)/collectl.conf
	$(INSTALL) -m 644 service/collectl.service $(PCP_SYSTEMDUNIT_DIR)/collectl.service
	$(INSTALL) -m 644 service/collectl-daily.service $(PCP_SYSTEMDUNIT_DIR)/collectl-daily.service
	$(INSTALL) -m 644 service/collectl-daily.timer $(PCP_SYSTEMDUNIT_DIR)/collectl-daily.timer
	$(INSTALL) -m 644 pcp-collectl.1 pcp-colmux.1 $(MANDEST)
	$(INSTALL) -S $(PCP_BINADM_DIR)/pcp-collectl $(PCP_BIN_DIR)/collectl
	$(INSTALL) -S $(PCP_BINADM_DIR)/pcp-colmux   $(PCP_BIN_DIR)/colmux
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

check :: $(MAN_PAGES)
	$(MANLINT) $^
