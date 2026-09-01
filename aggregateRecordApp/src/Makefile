# SPDX-FileCopyrightText: 1998 Argonne National Laboratory
#
# SPDX-License-Identifier: EPICS

TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS BELOW HERE

# Use typed rset structure (see 3.16.1 release notes)
USR_CPPFLAGS += -DUSE_TYPED_RSET -DUSE_TYPED_DSET -DUSE_TYPED_DRVET

# xxxRecord.h will be created from xxxRecord.dbd
DBDINC += xxxRecord

# Install xxxSupport.dbd into <top>/dbd
DBD += xxxSupport.dbd

# Build an IOC support library
LIBRARY_IOC += aggregateRecordSupport

# Compile and add code to the support library
aggregateRecordSupport_SRCS += xxxRecord.c
aggregateRecordSupport_SRCS += devXxxSoft.c

# Link locally-provided code into the support library,
# rather than directly into the IOC application, that
# causes problems on Windows DLL builds
aggregateRecordSupport_SRCS += devaggregateRecordVersion.c

aggregateRecordSupport_LIBS += $(EPICS_BASE_IOC_LIBS)

# Auto-generate a header file containing a version string.
# Version comes from the VCS if available, else date+time.
GENVERSION = aggregateRecordVersion.h
# Macro name
GENVERSIONMACRO = aggregateRecordVERSION

# Build the IOC application
PROD_IOC = aggregateRecord

# aggregateRecord.dbd will be created and installed
DBD += aggregateRecord.dbd

# aggregateRecord.dbd will include these files:
aggregateRecord_DBD += base.dbd
aggregateRecord_DBD += xxxSupport.dbd
aggregateRecord_DBD += devaggregateRecordVersion.dbd

# aggregateRecord_registerRecordDeviceDriver.cpp derives from aggregateRecord.dbd
aggregateRecord_SRCS += aggregateRecord_registerRecordDeviceDriver.cpp

# Build the main IOC entry point where needed
aggregateRecord_SRCS_DEFAULT += aggregateRecordMain.cpp
aggregateRecord_SRCS_vxWorks += -nil-

# Link in the code from our support library
aggregateRecord_LIBS += aggregateRecordSupport

# Link QSRV2
aggregateRecord_DBD += pvxsIoc.dbd
aggregateRecord_LIBS += pvxsIoc pvxs

# Finally link IOC to the EPICS Base libraries
aggregateRecord_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD EXTRA GNUMAKE RULES BELOW HERE

# Explicit dependency needed for generated header file
devaggregateRecordVersion$(DEP): $(COMMON_DIR)/$(GENVERSION)
