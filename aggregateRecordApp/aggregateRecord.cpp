/* SPDX-License-Identifier: BSD-3-Clause
 * See file: COPYRIGHT
 * Author: Bruno Martins
 *
 * Record support for the "aggregate" record type.
 *
 * The record is a generic carrier for NTAggregate: every process() asks device
 * support for a new observation via read_newvalue(), posts whichever statistics
 * fields it maintains, and then invokes the RPVT notify hook that drives the
 * PV Access update in AggregateSource.  Which statistics are computed, and over
 * how many samples, is entirely the device support's business (see
 * devAggregateSoft for the default).  Device support is mandatory.
 *
 * Writing N (special SPC_MOD) sets FLSH, which asks device support to finish
 * the current window on the next process() instead of taking a sample.
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
static long special(DBADDR *, int);
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

static void checkAlarms(aggregateRecord *prec)
{
    /* VAL is undefined until device support finishes its first window. */
    if (prec->udf)
        recGblSetSevr(prec, UDF_ALARM, prec->udfs);
}

static void monitor(aggregateRecord *prec, long updateVal)
{
    unsigned short monitor_mask = recGblResetAlarms(prec);

    /* Device support may have touched any of these; post them all rather than
       track per-field changes.  FTIME/LTIME are DBF_NOACCESS: nothing can
       subscribe to them, so they are not posted. */
    monitor_mask |= DBE_VALUE | DBE_LOG;
    db_post_events(prec, &prec->dpsr, monitor_mask);
    db_post_events(prec, &prec->idxn, monitor_mask);
    db_post_events(prec, &prec->fval, monitor_mask);
    db_post_events(prec, &prec->lval, monitor_mask);
    db_post_events(prec, &prec->maxv, monitor_mask);
    db_post_events(prec, &prec->minv, monitor_mask);

    if (updateVal)
        db_post_events(prec, &prec->val,  monitor_mask);
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

        /* MAXN sizes the device support's sample buffer and cannot change
           after init.  iocInit ignores our return value, so pact is what
           actually keeps a misconfigured record from ever processing. */
        if (prec->maxn < 1) {
            recGblRecordError(S_db_badField, prec,
                              "aggregate: init_record (MAXN must be >= 1)");
            prec->pact = TRUE;
            return S_db_badField;
        }
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

static long special(DBADDR *paddr, int after)
{
    aggregateRecord *prec = (aggregateRecord *)paddr->precord;

    if (!after)
        return 0;

    /* N is pp(TRUE): after this returns, dbPutField processes a Passive record
       and device support finishes the window instead of sampling. */
    if (dbGetFieldIndex(paddr) == aggregateRecordN)
        prec->flsh = 1;

    return 0;
}

static long process(struct dbCommon *pcommon)
{
    aggregateRecord *prec = (aggregateRecord *)pcommon;
    aggregatedset   *pdset = (aggregatedset *)prec->dset;
    unsigned char   pact = prec->pact;
    long            status = 0;
    long            updateType = 0;

    if (pdset == NULL || pdset->read_newvalue == NULL) {
        prec->pact = TRUE;
        recGblRecordError(S_dev_missingSup, prec, "read_newvalue");
        return S_dev_missingSup;
    }

    /* pact must not be set until after calling device support, so async device
       support can claim the record by setting it itself. */
    updateType = pdset->read_newvalue(prec);
    if (!pact && prec->pact)
        return 0;

    /* TODO: refactor the error handling */
    if ((updateType != 0) && (updateType != 2))
        status = updateType;

    prec->pact = TRUE;

    recGblGetTimeStamp(prec);

    /* udf is cleared by device support when it finishes a window, not here:
       a partial sample does not make VAL defined. */
    checkAlarms(prec);

    /* post updates to VAL only when measurement is finished */
    monitor(prec, updateType);
    if (updateType)
        notifyPublisher(prec);

    recGblFwdLink(prec);
    prec->pact = FALSE;
    return status;
}
