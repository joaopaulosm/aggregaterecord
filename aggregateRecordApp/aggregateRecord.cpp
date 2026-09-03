/* SPDX-License-Identifier: BSD-3-Clause
 * See file: COPYRIGHT
 * Author: Bruno Martins
 *
 * Record support for the "aggregate" record type.
 *
 * On every process() the record reads a new VAL through device support and then
 * invokes the RPVT notify hook, which is what drives the NTAggregate PV update
 * in AggregateSource.  Device support is mandatory; devAggregateSoft provides
 * the default "Soft Channel" that reads VAL from INP.
 */
#include <alarm.h>
#include <dbAccess.h>
#include <dbDefs.h>
#include <dbEvent.h>
#include <dbFldTypes.h>
#include <devSup.h>
#include <errMdef.h>
#include <errlog.h>
#include <recGbl.h>
#include <recSup.h>

#include "aggregateRecordHook.h"

#define GEN_SIZE_OFFSET
#include "aggregateRecord.h"
#undef GEN_SIZE_OFFSET

#include <epicsExport.h>   /* defines epicsExportSharedSymbols, keep last */

/* Create RSET - Record Support Entry Table */
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
#define special NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

rset aggregateRSET = {
    RSETNUMBER,
    report,
    initialize,
    init_record,
    process,
    special,
    get_value,
    cvt_dbaddr,
    get_array_info,
    put_array_info,
    get_units,
    get_precision,
    get_enum_str,
    get_enum_strs,
    put_enum_str,
    get_graphic_double,
    get_control_double,
    get_alarm_double
};
epicsExportAddress(rset, aggregateRSET);

static void monitor(aggregateRecord *prec)
{
    unsigned short monitor_mask = recGblResetAlarms(prec);

    /* VAL is regenerated every cycle, so always post it. */
    monitor_mask |= DBE_VALUE | DBE_LOG;
    db_post_events(prec, &prec->val, monitor_mask);
}

/* Hand the record to whoever installed itself in RPVT (AggregateSource).  Called
   with the record lock held, so the publisher sees a consistent VAL/TIME. */
static void notifyPublisher(aggregateRecord *prec)
{
    aggregateRecordPvt *hook = static_cast<aggregateRecordPvt *>(prec->rpvt);

    if (hook && hook->notify)
        hook->notify(prec);
}

static long init_record(struct dbCommon *pcommon, int pass)
{
    aggregateRecord *prec = (aggregateRecord *)pcommon;
    aggregatedset   *pdset;

    if (pass == 0) {
        /* AggregateSource fills this in at initHookAfterIocBuilt. */
        prec->rpvt = NULL;
        return 0;
    }

    // from this point we are in pass = 1 (when links were resolved)

    /* Device support is mandatory: it is the only thing that produces VAL. */
    pdset = (aggregatedset *)prec->dset;
    if (!pdset) {
        errlogPrintf("aggregateRecord '%s': no dset defined\n", prec->name);
        recGblRecordError(S_dev_noDSET, prec, "aggregate: init_record");
        return S_dev_noDSET;
    }

    if (pdset->common.number < 5 || pdset->read_newvalue == NULL) {
        errlogPrintf("aggregateRecord '%s': no read_newvalue defined\n", prec->name);
        recGblRecordError(S_dev_missingSup, prec, "aggregate: init_record");
        return S_dev_missingSup;
    }

    // start device support 
    if (pdset->common.init_record)
        return pdset->common.init_record(pcommon);

    return 0;
}

static long process(struct dbCommon *pcommon)
{
    aggregateRecord *prec = (aggregateRecord *)pcommon;
    aggregatedset   *pdset = (aggregatedset *)prec->dset;
    unsigned char   pact = prec->pact;
    long            status = 0;

    if (pdset == NULL || pdset->read_newvalue == NULL) {
        prec->pact = TRUE;
        recGblRecordError(S_dev_missingSup, prec, "read_newvalue");
        return S_dev_missingSup;
    }

    /* pact must not be set until after calling device support, so async device
       support can claim the record by setting it itself. */
    status = pdset->read_newvalue(prec);
    if (!pact && prec->pact)
        return 0;

    prec->pact = TRUE;

    recGblGetTimeStamp(prec);
    if (status == 0)
        prec->udf = FALSE;

    monitor(prec);
    notifyPublisher(prec);

    recGblFwdLink(prec);
    prec->pact = FALSE;
    return status;
}
