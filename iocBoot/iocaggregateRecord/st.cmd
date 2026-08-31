#!../../bin/darwin-x86/aggregateRec

#- SPDX-FileCopyrightText: 2000 Argonne National Laboratory
#-
#- SPDX-License-Identifier: EPICS

#- You may have to change aggregateRec to something else
#- everywhere it appears in this file

< envPaths

cd "${TOP}"

## Register all support components
dbLoadDatabase "dbd/aggregateRec.dbd"
aggregateRec_registerRecordDeviceDriver pdbbase

## Load record instances
dbLoadRecords "db/aggregateRecVersion.db", "user=joaopaulomartins"

cd "${TOP}/iocBoot/${IOC}"
iocInit
date
