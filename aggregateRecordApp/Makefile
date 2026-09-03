TOP=..
include $(TOP)/configure/CONFIG

USR_CPPFLAGS += -DUSE_TYPED_RSET -DUSE_TYPED_DSET -DDUSE_TYPED_DRVET
USR_CXXFLAGS += -std=c++11

DBDINC += aggregateRecord
DBD += aggregateSupport.dbd

LIBRARY_IOC += aggregateRecord

aggregateRecord_SRCS += aggregateRecord.cpp
aggregateRecord_SRCS += devAggregateSoft.cpp
aggregateRecord_SRCS += aggregateRecordUtil.cpp
aggregateRecord_SRCS += ntaggregate.cpp

aggregateRecord_SRCS += aggregateSource.cpp
aggregateRecord_SRCS += aggregateSourceRegistrar.cpp

aggregateRecord_LIBS += pvxsIoc pvxs $(EPICS_BASE_IOC_LIBS)

USR_CPPFLAGS += -DBUILDING_aggregateRecord_API
API_HEADER = aggregateRecordAPI.h
INC += aggregate/ntaggregate.h

include $(TOP)/configure/RULES

ifdef T_A
ifeq (,$(INSTALL_API_HEADERS))
$(error Requires epics-base >= 7.0.4)
endif
endif
