/* SPDX-License-Identifier: BSD-3-Clause
 * See file: COPYRIGHT
 * Author: Bruno Martins
 *
 * "Soft Channel" device support for the aggregate record.
 *
 * Accumulates successive samples from INP into a MAXN-sized buffer.  Every
 * sample updates the running extremes (FVAL/FTIME, MINV, MAXV) and IDXN so
 * partial results are published on each process(); when IDXN reaches N (or
 * FLSH is set) the window is finished: mean -> VAL, population standard
 * deviation -> DPSR, last sample -> LVAL/LTIME.  The next sample then starts a
 * new window.
 *
 * Link handling follows Base's devLiSoft: DB/CA links are read under
 * dbLinkDoLocked() so value and timestamp come from the same instant; a
 * CONSTANT INP is re-parsed on every process (dbGetLink() on a CONSTANT link
 * succeeds but writes nothing).
 */
#include <math.h>
#include <stdlib.h>

#include <alarm.h>
#include <dbAccess.h>
#include <dbDefs.h>
#include <devSup.h>
#include <epicsTime.h>
#include <errlog.h>
#include <recGbl.h>

#include "aggregateRecord.h"
#include "aggregateRecordUtil.h"

#include <epicsExport.h>   /* defines epicsExportSharedSymbols, keep last */

struct SoftPvt {
    double        *buf;     /* MAXN samples; allocated once, never freed */
    epicsUInt32    cap;     /* == MAXN */
    epicsTimeStamp lastTs;  /* stamp of the most recent sample */
    bool           done;    /* window finished: next sample starts a new one */
};

struct Sample {
    double         x;
    epicsTimeStamp ts;
};

static long init_record(dbCommon *pcommon);
static long read_newvalue(aggregateRecord *prec);

aggregatedset devAggregateSoft = {
    {5, NULL, NULL, init_record, NULL},
    read_newvalue
};
epicsExportAddress(dset, devAggregateSoft);

static SoftPvt *ensurePvt(aggregateRecord *prec)
{
    AggregateRecordWrapper rec(*prec);
    SoftPvt *pvt = rec.get_private<SoftPvt>();

    if (pvt)
        return pvt;

    if (prec->maxn < 1)
        return NULL;

    pvt = (SoftPvt *)calloc(1, sizeof(*pvt));
    if (!pvt)
        return NULL;

    pvt->buf = (double *)calloc(prec->maxn, sizeof(double));
    if (!pvt->buf) {
        free(pvt);
        return NULL;
    }
    pvt->cap = prec->maxn;
    /* Whatever IDXN says, a fresh buffer holds no samples: start over. */
    pvt->done = true;

    rec.set_private(pvt);
    return pvt;
}

/* Runs with the source record's lock held.  dbGetLink() raises LINK_ALARM
   itself on failure. */
static long readLocked(struct link *pinp, void *priv)
{
    Sample *s = (Sample *)priv;
    long status = dbGetLink(pinp, DBR_DOUBLE, &s->x, 0, 0);

    if (status)
        return status;

    if (dbGetTimeStamp(pinp, &s->ts))
        epicsTimeGetCurrent(&s->ts);

    return 0;
}

/* *have is false when the link is a CONSTANT that holds nothing (INP unset):
   not an error, just no observation. */
static long fetchSample(aggregateRecord *prec, Sample *s, bool *valid)
{
    long status;

    *valid = false;

    if (dbLinkIsConstant(&prec->inp)) {
        // indicate that no value was fetched
        if (dbLoadLink(&prec->inp, DBF_DOUBLE, &s->x))
            return 0;
        // if it is a constant, consider it valid and use current TS
        epicsTimeGetCurrent(&s->ts);
        *valid = true;
        return 0;
    }

    // get the value from the input link
    status = dbLinkDoLocked(&prec->inp, readLocked, s);
    if (status == S_db_noLSET)
        status = readLocked(&prec->inp, s);

    *valid = (status == 0);
    return status;
}

static long finishWindow(aggregateRecord *prec, SoftPvt *pvt)
{
    epicsUInt32 n = prec->idxn;

    // validate the number of observations
    if (n > pvt->cap)
        n = pvt->cap;

    if (n > 0) {
        double sum = 0.0, m2 = 0.0, mean;
        epicsUInt32 i;

        // calculate mean
        for (i = 0; i < n; i++)
            sum += pvt->buf[i];
        mean = sum / n;

        // calculate std dev
        for (i = 0; i < n; i++) {
            double d = pvt->buf[i] - mean;
            m2 += d * d;
        }

        // populate the main records
        prec->val   = mean;
        prec->dpsr  = sqrt(m2 / n);
        prec->lval  = pvt->buf[n - 1];
        prec->ltime = pvt->lastTs;
        prec->udf   = FALSE;
    }

    pvt->done = true;
    return 2;
}

static long accumulate(aggregateRecord *prec, SoftPvt *pvt, const Sample *s)
{
    epicsUInt32 target = prec->n;

    // validate the number of observations
    if (target < 1)
        target = 1;
    if (target > pvt->cap)
        target = pvt->cap;

    // 
    if (pvt->done || prec->idxn >= target) {
        prec->idxn = 0;
        pvt->done  = false;
    }

    // copy data and timestamp to the buffer
    pvt->buf[prec->idxn++] = s->x;
    pvt->lastTs = s->ts;

    // if first sample, reset statistics
    if (prec->idxn == 1) {
        prec->fval  = s->x;
        prec->minv  = s->x;
        prec->maxv  = s->x;
        prec->ftime = s->ts;
    } else {
        if (s->x < prec->minv)
            prec->minv = s->x;
        if (s->x > prec->maxv)
            prec->maxv = s->x;
    }

    /* With TSE=-2 the record's stamp is ours to set; use the observation's so
       partial updates are stamped by the sample that produced them. */
    if (dbLinkIsConstant(&prec->tsel) &&
        prec->tse == epicsTimeEventDeviceTime)
        prec->time = s->ts;

    // if observations are complete, perform the calculations
    if (prec->idxn >= target)
        return finishWindow(prec, pvt);

    return 0;
}

static long init_record(dbCommon *pcommon)
{
    aggregateRecord *prec = (aggregateRecord *)pcommon;

    if (!ensurePvt(prec)) {
        recGblRecordError(S_db_badField, prec,
                          "devAggregateSoft: init_record (MAXN < 1 or out of memory)");
        return S_db_badField;
    }
    return 0;
}

static long read_newvalue(aggregateRecord *prec)
{
    SoftPvt *pvt = ensurePvt(prec);
    Sample   s;
    bool     valid;
    long     status;

    if (!pvt) {
        recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
        return S_db_badField;
    }

    /* A write to N (via special) or to FLSH: finish what we have, take no
       sample.  The next sample opens a new window. */
    if (prec->flsh) {
        prec->flsh = 0;
        return finishWindow(prec, pvt);
    }

    status = fetchSample(prec, &s, &valid);
    if (status)
        return status;

    if (valid)
        return accumulate(prec, pvt, &s);

    return 0;
}
