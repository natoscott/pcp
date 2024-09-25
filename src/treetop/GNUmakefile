#
# Copyright (c) 2024 Red Hat.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

COPYRIGHT = "(C) 2004-2019 Hisham Muhammad. (C) 2020-2024 htop dev team."

CMDTARGET = treetop
DISTARGET = $(PCP_BINADM_DIR)/$(CMDTARGET)
LLDLIBS = $(PCP_MMVLIB) $(LIB_FOR_NCURSESW) $(LIB_FOR_MATH) $(LIB_FOR_BACKTRACE)
NCURSES_CFLAGS = -DHAVE_LIBNCURSESW=$(HAVE_NCURSESW)
LCFLAGS = $(NCURSES_CFLAGS) $(C99_CFLAGS) -Ipcp -I. -DBUILD_TREETOP=1
LCFLAGS += -DNDEBUG -DVERSION=\"$(PCP_VERSION)\" -DPACKAGE_NAME=\"treetop\"
LCFLAGS += -DSYSCONFDIR=\"$(PCP_SYSCONF_DIR)\" -DCONFIGDIR=\"/.pcp/treetop\"
LCFLAGS += -DCOPYRIGHT=\"$(COPYRIGHT)\" -DPACKAGE=\"$(CMDTARGET)\"
LCFLAGS += -D_XOPEN_SOURCE_EXTENDED -DMAJOR_IN_SYSMACROS=1 -DHTOP_PCP=1

TOPCFILES = \
	AvailableColumnsPanel.c \
	AvailableMetersPanel.c \
	CategoriesPanel.c \
	ClockMeter.c \
	ColorsPanel.c \
	ColumnsPanel.c \
	CommandLine.c \
	Compat.c \
	CPUMeter.c \
	CRT.c \
	DateMeter.c \
	DateTimeMeter.c \
	DisplayOptionsPanel.c \
	DynamicColumn.c \
	DynamicMeter.c \
	DynamicScreen.c \
	FunctionBar.c \
	Hashtable.c \
	Header.c \
	HeaderOptionsPanel.c \
	HostnameMeter.c \
	IncSet.c \
	ListItem.c \
	Machine.c \
	MainPanel.c \
	Meter.c \
	MetersPanel.c \
	Object.c \
	OptionItem.c \
	Panel.c \
	RichString.c \
	Row.c \
	Scheduling.c \
	ScreenManager.c \
	ScreensPanel.c \
	ScreenTabsPanel.c \
	Settings.c \
	SignalsPanel.c \
	Table.c \
	UsersTable.c \
	Vector.c \
	XUtils.c \

SUBCFILES = \
	linux/CGroupUtils.c \

TOPHFILES = \
	AvailableColumnsPanel.h \
	AvailableMetersPanel.h \
	CategoriesPanel.h \
	ClockMeter.h \
	ColorsPanel.h \
	ColumnsPanel.h \
	CommandLine.h \
	Compat.h \
	CPUMeter.h \
	CRT.h \
	DateMeter.h \
	DateTimeMeter.h \
	DisplayOptionsPanel.h \
	DynamicColumn.h \
	DynamicMeter.h \
	DynamicScreen.h \
	FunctionBar.h \
	Hashtable.h \
	Header.h \
	HeaderLayout.h \
	HeaderOptionsPanel.h \
	HostnameMeter.h \
	IncSet.h \
	ListItem.h \
	Machine.h \
	Macros.h \
	MainPanel.h \
	Meter.h \
	MeterMode.h \
	MetersPanel.h \
	Object.h \
	OptionItem.h \
	Panel.h \
	ProvideCurses.h \
	ProvideTerm.h \
	RichString.h \
	Row.h \
	RowField.h \
	Scheduling.h \
	ScreenManager.h \
	ScreensPanel.h \
	ScreenTabsPanel.h \
	Settings.h \
	SignalsPanel.h \
	Table.h \
	UsersTable.h \
	Vector.h \
	XUtils.h \

SUBHFILES = \
	linux/CGroupUtils.h \

TREETOPCFILES = \
	treetop.c \
	pcp/Action.c \
	pcp/Metric.c \
	pcp/TreeTop.c \
	pcp/PCPMachine.c \
	pcp/ConfidenceMeter.c \
	pcp/ElapsedTimeMeter.c \
	pcp/FeaturesMeter.c \
	pcp/Feature.c \
	pcp/FeatureTable.c \
	pcp/ProcessingStateMeter.c \
	pcp/SampleIntervalMeter.c \
	pcp/SamplingTimeMeter.c \
	pcp/TargetMetricMeter.c \
	pcp/TargetTimestampMeter.c \
	pcp/TargetValueMeter.c \
	pcp/TrainingTimeMeter.c \
	pcp/TrainingWindowMeter.c \

TREETOPHFILES = \
	pcp/Action.h \
	pcp/Metric.h \
	pcp/TreeTop.h \
	pcp/PCPMachine.h \
	pcp/ConfidenceMeter.h \
	pcp/ElapsedTimeMeter.h \
	pcp/FeaturesMeter.h \
	pcp/Feature.h \
	pcp/FeatureTable.h \
	pcp/ProcessingStateMeter.h \
	pcp/SampleIntervalMeter.h \
	pcp/SamplingTimeMeter.h \
	pcp/TargetMetricMeter.h \
	pcp/TargetTimestampMeter.h \
	pcp/TargetValueMeter.h \
	pcp/TrainingTimeMeter.h \
	pcp/TrainingWindowMeter.h \

DOCFILES = AUTHORS

TOPXFILES = $(TOPCFILES) $(TOPHFILES) $(DOCFILES)
SUBXFILES = $(SUBCFILES) $(SUBHFILES)
CFILES = $(TREETOPCFILES) $(TOPCFILES) $(SUBCFILES)
HFILES = $(TREETOPHFILES) $(TOPHFILES) $(SUBHFILES)
LDIRT = $(TOPXFILES) $(SUBXFILES)

#MAN_PAGES = $(CMDTARGET).1 $(CMDTARGET).5

default:	build-me

include $(BUILDRULES)

ifeq "$(HAVE_NCURSESW)" "true"
build-me: $(TOPXFILES) $(SUBXFILES) $(CMDTARGET) $(MAN_PAGES)

install: default
	$(INSTALL) -m 755 $(CMDTARGET) $(DISTARGET)
#	$(INSTALL_MAN)
else
build-me:
install:
endif

default_pcp: default

install_pcp: install

$(TOPXFILES):
	$(LN_S) -f $(TOPDIR)/vendor/github.com/htop-dev/htop/$@ $@

$(SUBXFILES):
	mkdir -p linux pcp
	$(LN_S) -f $(TOPDIR)/../vendor/github.com/htop-dev/htop/$@ $@
