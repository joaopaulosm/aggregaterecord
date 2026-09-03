/* SPDX-License-Identifier: BSD-3-Clause
 * See file: COPYRIGHT
 * Author: Bruno Martins
 *
 * "Soft Channel" device support for the aggregate record.
 *
 * Fetches a scalar from INP and places it in VAL, which record support then
 * publishes as the "value" field of the NTAggregate PV.  Modelled on Base's
 * devLiSoft/devAiSoft: a CONSTANT INP is read once at init, anything else is
 * read on every process().
 */
#include <alarm.h>
#include <dbAccess.h>
#include <dbDefs.h>
#include <devSup.h>
#include <epicsTime.h>
#include <recGbl.h>

#include "aggregateRecord.h"

#include <epicsExport.h>   /* defines epicsExportSharedSymbols, keep last */

static long init_record(dbCommon *pcommon);
static long read_newvalue(aggregateRecord *prec);

aggregatedset devAggregateSoft = {
    {5, NULL, NULL, init_record, NULL},
    read_newvalue
};
epicsExportAddress(dset, devAggregateSoft);

static long init_record(dbCommon *pcommon)
{
    aggregateRecord *prec = (aggregateRecord *)pcommon;

    /* A CONSTANT link supplies VAL once, here; read_newvalue() then has
       nothing to do on each process(). */
    if (recGblInitConstantLink(&prec->inp, DBF_DOUBLE, &prec->val))
        prec->udf = FALSE;

    return 0;
}

/* Runs with the source record's lock held, so VAL and TIME come from the same
   instant.  dbGetLink() raises LINK_ALARM itself on failure. */
static long readLocked(struct link *pinp, void *)
{
    aggregateRecord *prec = (aggregateRecord *)pinp->precord;
    long status = dbGetLink(pinp, DBR_DOUBLE, &prec->val, 0, 0);

    if (status)
        return status;

    if (dbLinkIsConstant(&prec->tsel) &&
        prec->tse == epicsTimeEventDeviceTime)
        dbGetTimeStamp(pinp, &prec->time);

    return status;
}

static long read_newvalue(aggregateRecord *prec)
{
    long status;

    /* VAL was already loaded by init_record(); nothing to re-read. */
    if (dbLinkIsConstant(&prec->inp))
        return 0;

    status = dbLinkDoLocked(&prec->inp, readLocked, NULL);
    if (status == S_db_noLSET)
        status = readLocked(&prec->inp, NULL);

    return status;
}
