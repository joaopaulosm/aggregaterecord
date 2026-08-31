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
LIBRARY_IOC += aggregateRecSupport

# Compile and add code to the support library
aggregateRecSupport_SRCS += xxxRecord.c
aggregateRecSupport_SRCS += devXxxSoft.c

# Link locally-provided code into the support library,
# rather than directly into the IOC application, that
# causes problems on Windows DLL builds
aggregateRecSupport_SRCS += devaggregateRecVersion.c

aggregateRecSupport_LIBS += $(EPICS_BASE_IOC_LIBS)

# Auto-generate a header file containing a version string.
# Version comes from the VCS if available, else date+time.
GENVERSION = aggregateRecVersion.h
# Macro name
GENVERSIONMACRO = aggregateRecVERSION

# Build the IOC application
PROD_IOC = aggregateRec

# aggregateRec.dbd will be created and installed
DBD += aggregateRec.dbd

# aggregateRec.dbd will include these files:
aggregateRec_DBD += base.dbd
aggregateRec_DBD += xxxSupport.dbd
aggregateRec_DBD += devaggregateRecVersion.dbd

# aggregateRec_registerRecordDeviceDriver.cpp derives from aggregateRec.dbd
aggregateRec_SRCS += aggregateRec_registerRecordDeviceDriver.cpp

# Build the main IOC entry point where needed
aggregateRec_SRCS_DEFAULT += aggregateRecMain.cpp
aggregateRec_SRCS_vxWorks += -nil-

# Link in the code from our support library
aggregateRec_LIBS += aggregateRecSupport

# Link QSRV2
aggregateRec_DBD += pvxsIoc.dbd
aggregateRec_LIBS += pvxsIoc pvxs

# Finally link IOC to the EPICS Base libraries
aggregateRec_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD EXTRA GNUMAKE RULES BELOW HERE

# Explicit dependency needed for generated header file
devaggregateRecVersion$(DEP): $(COMMON_DIR)/$(GENVERSION)
