#!../../bin/darwin-x86/aggregateExample

#- SPDX-FileCopyrightText: 2000 Argonne National Laboratory
#-
#- SPDX-License-Identifier: EPICS

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/aggregateExample.dbd"
aggregateExample_registerRecordDeviceDriver pdbbase

## Load record instances
dbLoadRecords "db/aggregateExample.db", "P=TST:"

cd "${TOP}/iocBoot/${IOC}"
iocInit

## Each aggregate record now serves an epics:nt/NTAggregate:1.0 PV of the
## same name, e.g.  pvxget TST:AGG1
