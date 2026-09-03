# SPDX-FileCopyrightText: 1997 Argonne National Laboratory
#
# SPDX-License-Identifier: EPICS

TOP=..
include $(TOP)/configure/CONFIG

#==================================================
# Build an IOC to exercise the aggregate record type

PROD_IOC = aggregateExample

# aggregateExample.dbd is created from the includes below and installed
DBD += aggregateExample.dbd

aggregateExample_DBD += base.dbd
aggregateExample_DBD += aggregateSupport.dbd

# aggregateExample_registerRecordDeviceDriver.cpp derives from aggregateExample.dbd
aggregateExample_SRCS += aggregateExample_registerRecordDeviceDriver.cpp

# Build the main IOC entry point where needed
aggregateExample_SRCS_DEFAULT += aggregateExampleMain.cpp
aggregateExample_SRCS_vxWorks += -nil-

# The support library under test, then QSRV2, then Base
aggregateExample_LIBS += aggregateRecord
aggregateExample_LIBS += pvxsIoc pvxs
aggregateExample_LIBS += $(EPICS_BASE_IOC_LIBS)

# Example database
DB += aggregateExample.db

include $(TOP)/configure/RULES
